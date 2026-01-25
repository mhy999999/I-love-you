// MusicController 实现：打通搜索结果到 QtMultimedia 播放
#include "music_controller.h"

#include <QCoreApplication>
#include <QAudioOutput>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QTcpServer>
#include <QThread>
#include <QTimer>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>

#include "logger.h"

namespace App
{

namespace
{

QString redactSensitive(QString s)
{
	QRegularExpression re(QStringLiteral("(SESSDATA|bili_jct|bili_ticket|bili_ticket_expires|DedeUserID|DedeUserID__ckMd5)=[^;\\s]*"));
	s.replace(re, QStringLiteral("\\1=<redacted>"));
	return s;
}

bool probeHttp(const QUrl &url, int timeoutMs)
{
	QNetworkAccessManager manager;
	QNetworkRequest request(url);
	request.setRawHeader("Accept", "application/json");
	QNetworkReply *reply = manager.get(request);

	QEventLoop loop;
	QTimer timer;
	timer.setSingleShot(true);
	QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
		if (reply->isRunning())
			reply->abort();
		loop.quit();
	});
	QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
		loop.quit();
	});
	if (timeoutMs > 0)
		timer.start(timeoutMs);
	loop.exec();

	bool ok = (reply->error() == QNetworkReply::NoError) && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid();
	reply->deleteLater();
	return ok;
}

bool isPortAvailable(int port)
{
	QTcpServer server;
	bool ok = server.listen(QHostAddress::LocalHost, static_cast<quint16>(port));
	server.close();
	return ok;
}

QString findEmbeddedMusicApiDir(const QString &preferredDir)
{
	if (!preferredDir.trimmed().isEmpty())
	{
		QString dir = QDir::fromNativeSeparators(preferredDir.trimmed());
		if (QFileInfo::exists(QDir(dir).filePath(QStringLiteral("package.json"))))
			return dir;
	}

	QByteArray envDir = qgetenv("QTREWRITE_MUSIC_API_DIR");
	if (!envDir.isEmpty())
	{
		QString dir = QDir::fromNativeSeparators(QString::fromLocal8Bit(envDir));
		if (QFileInfo::exists(QDir(dir).filePath(QStringLiteral("package.json"))))
			return dir;
	}

	const QStringList roots = {
		QDir::currentPath(),
		QCoreApplication::applicationDirPath(),
		QStringLiteral("D:/project/I-love-you/qt-rewrite")
	};
	for (const QString &root : roots)
	{
		QDir dir(root);
		for (int depth = 0; depth < 10; ++depth)
		{
			QString candidate = dir.filePath(QStringLiteral("qt-rewrite/music-api"));
			if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("package.json"))))
				return candidate;
			candidate = dir.filePath(QStringLiteral("music-api"));
			if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("package.json"))))
				return candidate;
			if (!dir.cdUp())
				break;
		}
	}
	return QString();
}

bool ensureEmbeddedMusicApiDeps(const QString &apiDir, bool autoInstall)
{
	QDir dir(apiDir);
	QString nodeModulesPath = dir.filePath(QStringLiteral("node_modules"));
	QString ncmApiPkg = dir.filePath(QStringLiteral("node_modules/netease-cloud-music-api-alger/package.json"));
	QString unblockPkg = dir.filePath(QStringLiteral("node_modules/@unblockneteasemusic/server/package.json"));
	bool depsOk = QFileInfo::exists(ncmApiPkg) && QFileInfo::exists(unblockPkg);
	if (depsOk)
		return true;

	if (!autoInstall)
	{
		Logger::warning(QStringLiteral("Embedded music API dependencies not installed: %1").arg(nodeModulesPath));
		return false;
	}

	QString lockFile = dir.filePath(QStringLiteral("package-lock.json"));
	QStringList args;
	if (QFileInfo::exists(lockFile))
		args = {QStringLiteral("ci")};
	else
		args = {QStringLiteral("install")};

	Logger::info(QStringLiteral("Installing embedded music API dependencies via npm %1").arg(args.join(' ')));
	QProcess npm;
	npm.setWorkingDirectory(apiDir);
	npm.setProcessChannelMode(QProcess::MergedChannels);
	npm.setProgram(QStringLiteral("npm"));
	npm.setArguments(args);
	npm.start();
	if (!npm.waitForStarted(1500))
	{
		Logger::warning(QStringLiteral("Failed to start npm: %1").arg(npm.errorString()));
		return false;
	}

	if (!npm.waitForFinished(15 * 60 * 1000))
	{
		npm.kill();
		npm.waitForFinished(1500);
		Logger::warning(QStringLiteral("npm did not finish within timeout"));
		return false;
	}

	QString output = QString::fromLocal8Bit(npm.readAll());
	if (npm.exitStatus() != QProcess::NormalExit || npm.exitCode() != 0)
	{
		QString tail = output.right(4000);
		Logger::warning(QStringLiteral("npm failed (code %1): %2").arg(npm.exitCode()).arg(tail));
		return false;
	}

	if (!QFileInfo::exists(nodeModulesPath))
	{
		QString tail = output.right(2000);
		Logger::warning(QStringLiteral("npm succeeded but node_modules still missing: %1 %2").arg(nodeModulesPath).arg(tail));
		return false;
	}
	return true;
}

bool startNeteaseMusicApiOnPort(QObject *parent, int port, const QString &apiDirOverride, bool autoInstall, QProcess **outProcess)
{
	QString apiDir = findEmbeddedMusicApiDir(apiDirOverride);
	if (apiDir.isEmpty())
	{
		Logger::warning(QStringLiteral("Embedded music API directory not found; cannot start local music API"));
		return false;
	}

	QString anonTokenPath = QDir(QDir::tempPath()).filePath(QStringLiteral("anonymous_token"));
	if (!QFileInfo::exists(anonTokenPath))
	{
		QFile f(anonTokenPath);
		if (f.open(QIODevice::WriteOnly))
			f.close();
	}

	if (!ensureEmbeddedMusicApiDeps(apiDir, autoInstall))
		return false;

	QProcess *p = new QProcess(parent);
	p->setWorkingDirectory(apiDir);
	p->setProcessChannelMode(QProcess::MergedChannels);

	QString js = QStringLiteral(
		"const mod=require('netease-cloud-music-api-alger/server');"
		"const api=mod.default||mod;"
		"console.log('Starting Netease API on port %1');"
		"api.serveNcmApi({port:%1, checkVersion:true}).then(()=>console.log('API Started')).catch(e=>{console.error(e);process.exit(1);});"
	).arg(port);

	p->setProgram(QStringLiteral("node"));
	p->setArguments({QStringLiteral("-e"), js});
	p->start();
	if (!p->waitForStarted(1500))
	{
		Logger::warning(QStringLiteral("Failed to start local music API process: %1").arg(p->errorString()));
		p->deleteLater();
		return false;
	}

	QElapsedTimer elapsed;
	elapsed.start();
	while (elapsed.elapsed() < 6000)
	{
		if (p->state() == QProcess::NotRunning)
		{
			QString out = QString::fromLocal8Bit(p->readAll());
			if (!out.isEmpty())
				Logger::warning(QStringLiteral("Local music API process exited: %1").arg(redactSensitive(out).right(2000)));
			break;
		}
		if (probeHttp(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(port)), 300))
		{
			if (outProcess)
				*outProcess = p;
			return true;
		}
		QThread::msleep(150);
	}
	if (p->state() != QProcess::NotRunning)
	{
		p->terminate();
		p->waitForFinished(800);
		if (p->state() != QProcess::NotRunning)
			p->kill();
	}
	p->deleteLater();
	return false;
}

QUrl resolveLocalMusicApiBaseUrl()
{
	QSettings settings;
	settings.beginGroup(QStringLiteral("set"));
	int startPort = settings.value(QStringLiteral("musicApiPort"), 30490).toInt();
	settings.endGroup();

	int port = startPort;
	const int maxRetries = 10;
	const int probeTimeoutMs = 400;
	for (int i = 0; i < maxRetries; ++i)
	{
		int candidate = startPort + i;
		QUrl url(QStringLiteral("http://127.0.0.1:%1/").arg(candidate));
		if (probeHttp(url, probeTimeoutMs))
		{
			port = candidate;
			break;
		}
	}

	if (port == startPort)
	{
		bool anyAlive = probeHttp(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(startPort)), probeTimeoutMs);
		if (!anyAlive)
		{
			int startCandidate = startPort;
			for (int i = 0; i < maxRetries; ++i)
			{
				int candidate = startPort + i;
				if (isPortAvailable(candidate))
				{
					startCandidate = candidate;
					break;
				}
			}
			if (startCandidate != startPort)
			{
				settings.beginGroup(QStringLiteral("set"));
				settings.setValue(QStringLiteral("musicApiPort"), startCandidate);
				settings.endGroup();
				port = startCandidate;
			}
		}
	}

	if (port != startPort)
	{
		settings.beginGroup(QStringLiteral("set"));
		settings.setValue(QStringLiteral("musicApiPort"), port);
		settings.endGroup();
	}

	return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));
}

}

MusicController::MusicController(QObject *parent)
	: QObject(parent)
	, httpClient(this)
	, providerManager(this)
	, m_songsModel(this)
	, m_lyricModel(this)
	, m_playlistModel(this)
	, m_queueModel(this)
	, m_player(this)
	, imageCache(QStringLiteral("images"), 200LL * 1024 * 1024)
	, lyricCache(QStringLiteral("lyrics"), 20LL * 1024 * 1024)
{
	QMap<QByteArray, QByteArray> headers;
	headers.insert("Accept", "application/json");
	httpClient.setDefaultHeaders(headers);
	httpClient.setUserAgent(QByteArray("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/123.0.0.0 Safari/537.36 Edg/123.0.0.0"));

	QSettings settings;
	settings.beginGroup(QStringLiteral("set"));
	QString apiBaseStr = settings.value(QStringLiteral("musicApiBaseUrl"), QString()).toString().trimmed();
	QString apiDirOverride = settings.value(QStringLiteral("musicApiDir"), QString()).toString().trimmed();
	bool autoStart = settings.value(QStringLiteral("musicApiAutoStart"), true).toBool();
	bool autoInstall = settings.value(QStringLiteral("musicApiAutoInstall"), true).toBool();
	int playbackMode = settings.value(QStringLiteral("playbackMode"), static_cast<int>(Sequence)).toInt();
	settings.endGroup();
	if (playbackMode < static_cast<int>(Sequence) || playbackMode > static_cast<int>(LoopOne))
		playbackMode = static_cast<int>(Sequence);
	m_playbackMode = playbackMode;

	bool explicitBaseUrl = !apiBaseStr.isEmpty();
	QUrl apiBase = explicitBaseUrl ? QUrl(apiBaseStr) : resolveLocalMusicApiBaseUrl();
	if (!apiBase.isValid())
		apiBase = resolveLocalMusicApiBaseUrl();

	if (!probeHttp(apiBase.resolved(QUrl(QStringLiteral("/"))), 400))
	{
		bool shouldAutoStart = autoStart;
		if (explicitBaseUrl)
		{
			QString host = apiBase.host().toLower();
			shouldAutoStart = autoStart && (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost"));
		}

		if (shouldAutoStart)
		{
			int port = apiBase.port(30488);
			Logger::info(QStringLiteral("Local music API not detected, trying to start on port %1").arg(port));
			if (startNeteaseMusicApiOnPort(this, port, apiDirOverride, autoInstall, &musicApiProcess))
			{
				apiBase = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));
				Logger::info(QStringLiteral("Local music API started on port %1").arg(port));
			}
			else
			{
				Logger::warning(QStringLiteral("Failed to start local music API"));
			}
		}
		else if (explicitBaseUrl)
		{
			Logger::warning(QStringLiteral("Music API not reachable: %1").arg(apiBase.toString()));
		}
	}
	neteaseProvider = new NeteaseProvider(&httpClient, apiBase, &providerManager);
	gdStudioProvider = new GdStudioProvider(&httpClient, &providerManager);
	providerManager.registerProvider(neteaseProvider);
	providerManager.registerProvider(gdStudioProvider);
	ProviderManagerConfig cfg;
	cfg.providerOrder = QStringList() << neteaseProvider->id() << gdStudioProvider->id();
	cfg.fallbackEnabled = true;
	providerManager.setConfig(cfg);

	settings.beginGroup(QStringLiteral("auth"));
	QString cookie = settings.value(QStringLiteral("cookie")).toString();
	settings.endGroup();
	if (!cookie.isEmpty())
	{
		neteaseProvider->setCookie(cookie);
		checkLoginStatus();
	}

	m_player.setAudioOutput(new QAudioOutput(this));
	
	settings.beginGroup(QStringLiteral("set"));
	int savedVolume = settings.value(QStringLiteral("volume"), 50).toInt();
	settings.endGroup();
	m_player.audioOutput()->setVolume(savedVolume / 100.0);

	QObject::connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
		setPlaying(state == QMediaPlayer::PlayingState);
	});
	QObject::connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
		if (status == QMediaPlayer::EndOfMedia)
			playNextInternal(false);
	});
	QObject::connect(&m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
		m_positionMs = pos;
		emit positionMsChanged();
		updateCurrentLyricIndexByPosition(pos);
	});
	QObject::connect(&m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
		m_durationMs = dur;
		emit durationMsChanged();
	});
	QObject::connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
		Logger::error(QStringLiteral("Player error: %1 - %2").arg(error).arg(errorString));
		emit errorOccurred(errorString);
	});

	connect(&m_playlistModel, &SongListModel::rowRequested, this, &MusicController::onPlaylistRowRequested);

	loadQueueFromSettings();

	connect(this, &MusicController::loginSuccess, this, &MusicController::loadUserPlaylist);

    connect(&m_queueModel, &SongListModel::itemMoved, this, [this](int from, int to) {
        if (m_currentSongIndex < 0) {
            saveQueueToSettings();
            return;
        }
        
        if (from == m_currentSongIndex) {
            m_currentSongIndex = to;
            emit currentSongIndexChanged();
        } else {
            if (from < m_currentSongIndex && to >= m_currentSongIndex) {
                m_currentSongIndex--;
                emit currentSongIndexChanged();
            } else if (from > m_currentSongIndex && to <= m_currentSongIndex) {
                m_currentSongIndex++;
                emit currentSongIndexChanged();
            }
        }
        saveQueueToSettings();
    });
}

MusicController::~MusicController()
{
	if (musicApiProcess)
	{
		if (musicApiProcess->state() != QProcess::NotRunning)
		{
			musicApiProcess->terminate();
			if (!musicApiProcess->waitForFinished(1200))
			{
				musicApiProcess->kill();
				musicApiProcess->waitForFinished(600);
			}
		}
		delete musicApiProcess;
		musicApiProcess = nullptr;
	}
}

SongListModel *MusicController::songsModel()
{
	return &m_songsModel;
}

LyricListModel *MusicController::lyricModel()
{
	return &m_lyricModel;
}

SongListModel *MusicController::playlistModel()
{
	return &m_playlistModel;
}

SongListModel *MusicController::queueModel() const
{
	return const_cast<SongListModel*>(&m_queueModel);
}

PlaylistListModel *MusicController::userPlaylistModel()
{
	return &m_userPlaylistModel;
}

PlaylistListModel *MusicController::createdPlaylistModel()
{
	return &m_createdPlaylistModel;
}

PlaylistListModel *MusicController::collectedPlaylistModel()
{
	return &m_collectedPlaylistModel;
}

bool MusicController::loading() const
{
	return m_loading;
}

QUrl MusicController::currentUrl() const
{
	return m_currentUrl;
}

bool MusicController::playing() const
{
	return m_playing;
}

int MusicController::playbackMode() const
{
	return m_playbackMode;
}

void MusicController::setPlaybackMode(int mode)
{
	if (mode < static_cast<int>(Sequence) || mode > static_cast<int>(LoopOne))
		mode = static_cast<int>(Sequence);
	if (m_playbackMode == mode)
		return;
	m_playbackMode = mode;
	QSettings settings;
	settings.beginGroup(QStringLiteral("set"));
	settings.setValue(QStringLiteral("playbackMode"), m_playbackMode);
	settings.endGroup();
	emit playbackModeChanged();
}

int MusicController::volume() const
{
	if (!m_player.audioOutput())
		return 50;
	return static_cast<int>(m_player.audioOutput()->volume() * 100.0);
}

qint64 MusicController::positionMs() const
{
	return m_positionMs;
}

qint64 MusicController::durationMs() const
{
	return m_durationMs;
}

int MusicController::currentSongIndex() const
{
	return m_currentSongIndex;
}

int MusicController::currentLyricIndex() const
{
	return m_currentLyricIndex;
}

qint64 MusicController::lyricOffsetMs() const
{
	return m_lyricOffsetMs;
}

void MusicController::setLyricOffsetMs(qint64 v)
{
	if (m_lyricOffsetMs == v)
		return;
	m_lyricOffsetMs = v;
	emit lyricOffsetMsChanged();
	updateCurrentLyricIndexByPosition(m_player.position());
}

qint64 MusicController::currentLyricStartMs() const
{
	const QList<LyricLine> &lines = m_lyricModel.lyric().lines;
	if (m_currentLyricIndex < 0 || m_currentLyricIndex >= lines.size())
		return 0;
	return lines.at(m_currentLyricIndex).timeMs;
}

qint64 MusicController::currentLyricNextMs() const
{
	const QList<LyricLine> &lines = m_lyricModel.lyric().lines;
	if (lines.isEmpty())
		return 0;
	int nextIndex = m_currentLyricIndex + 1;
	if (nextIndex >= 0 && nextIndex < lines.size())
		return lines.at(nextIndex).timeMs;
	// 若无下一行，则以歌曲总时长作为下一时间边界
	return m_durationMs > 0 ? m_durationMs : lines.last().timeMs;
}
QUrl MusicController::coverSource() const
{
	return m_coverSource;
}

QString MusicController::currentSongTitle() const
{
	return m_currentSongTitle;
}

QString MusicController::currentSongArtists() const
{
	return m_currentSongArtists;
}

bool MusicController::playlistLoading() const
{
	return m_playlistLoading;
}

QString MusicController::playlistName() const
{
	return m_playlistName;
}

bool MusicController::playlistHasMore() const
{
	return m_playlistHasMore;
}

void MusicController::setVolume(int v)
{
	if (!m_player.audioOutput())
		return;
	double clamped = qBound(0, v, 100) / 100.0;
	m_player.audioOutput()->setVolume(clamped);

	QSettings settings;
	settings.beginGroup(QStringLiteral("set"));
	settings.setValue(QStringLiteral("volume"), static_cast<int>(clamped * 100));
	settings.endGroup();

	emit volumeChanged();
}

void MusicController::setLoading(bool v)
{
	if (m_loading == v)
		return;
	m_loading = v;
	emit loadingChanged();
}

void MusicController::setCurrentUrl(const QUrl &url)
{
	if (m_currentUrl == url)
		return;
	m_currentUrl = url;
	m_player.setSource(m_currentUrl);
	emit currentUrlChanged();
}

void MusicController::setPositionMs(qint64 v)
{
	if (m_positionMs == v)
		return;
	m_positionMs = v;
	emit positionMsChanged();
}

void MusicController::setDurationMs(qint64 v)
{
	if (m_durationMs == v)
		return;
	m_durationMs = v;
	emit durationMsChanged();
}

void MusicController::setCurrentSongIndex(int v)
{
	if (m_currentSongIndex == v)
		return;
	m_currentSongIndex = v;
	emit currentSongIndexChanged();
}

void MusicController::setCurrentLyricIndex(int v)
{
	if (m_currentLyricIndex == v)
		return;
	m_currentLyricIndex = v;
	emit currentLyricIndexChanged();
}

void MusicController::setCoverSource(const QUrl &url)
{
	if (m_coverSource == url)
		return;
	m_coverSource = url;
	emit coverSourceChanged();
}

void MusicController::setPlaylistLoading(bool v)
{
	if (m_playlistLoading == v)
		return;
	m_playlistLoading = v;
	emit playlistLoadingChanged();
}

void MusicController::setPlaylistName(const QString &name)
{
	if (m_playlistName == name)
		return;
	m_playlistName = name;
	emit playlistNameChanged();
}

void MusicController::setPlaylistHasMore(bool v)
{
	if (m_playlistHasMore != v)
	{
		m_playlistHasMore = v;
		emit playlistHasMoreChanged();
	}
}

void MusicController::setPlaylistPageSize(int size)
{
    if (size < 5) size = 5; // 最小安全值
    if (m_playlistPageSize != size) {
        m_playlistPageSize = size;
        emit playlistPageSizeChanged();
    }
}

int MusicController::playlistPageSize() const
{
    return m_playlistPageSize;
}

void MusicController::setCurrentSongTitle(const QString &title)
{
	if (m_currentSongTitle == title)
		return;
	m_currentSongTitle = title;
	emit currentSongTitleChanged();
}

void MusicController::setCurrentSongArtists(const QString &artists)
{
	if (m_currentSongArtists == artists)
		return;
	m_currentSongArtists = artists;
	emit currentSongArtistsChanged();
}

void MusicController::updateCurrentLyricIndexByPosition(qint64 posMs)
{
	qint64 effectivePosMs = posMs + m_lyricOffsetMs;
	if (effectivePosMs < 0)
		effectivePosMs = 0;
	const QList<LyricLine> &lines = m_lyricModel.lyric().lines;
	if (lines.isEmpty())
	{
		setCurrentLyricIndex(-1);
		return;
	}
	bool anyTimestamp = false;
	for (const LyricLine &l : lines)
	{
		if (l.timeMs > 0)
		{
			anyTimestamp = true;
			break;
		}
	}
	if (!anyTimestamp)
	{
		setCurrentLyricIndex(0);
		return;
	}
	int lo = 0;
	int hi = lines.size() - 1;
	int best = 0;
	while (lo <= hi)
	{
		int mid = (lo + hi) / 2;
		qint64 t = lines.at(mid).timeMs;
		if (t <= effectivePosMs)
		{
			best = mid;
			lo = mid + 1;
		}
		else
		{
			hi = mid - 1;
		}
	}
	setCurrentLyricIndex(best);
}

void MusicController::clearLyric()
{
	Lyric empty;
	m_lyricModel.setLyric(empty);
	setCurrentLyricIndex(-1);
}

bool MusicController::lyricFromCache(const QString &key, Lyric &outLyric)
{
	QByteArray bytes;
	if (!lyricCache.get(key, bytes))
		return false;
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return false;
	QJsonObject root = doc.object();
	QJsonArray arr = root.value(QStringLiteral("lines")).toArray();
	Lyric lyric;
	lyric.lines.reserve(arr.size());
	for (const QJsonValue &v : arr)
	{
		QJsonObject o = v.toObject();
		LyricLine ll;
		ll.timeMs = static_cast<qint64>(o.value(QStringLiteral("t")).toDouble());
		ll.text = o.value(QStringLiteral("x")).toString();
		QString trimmed = ll.text.trimmed();
		if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('[')))
			continue;
		lyric.lines.append(ll);
	}
	if (lyric.lines.isEmpty())
		return false;
	bool metaOnly = true;
	for (const LyricLine &ll : lyric.lines)
	{
		QString t = ll.text.trimmed();
		bool isMeta = (t.contains(QStringLiteral("\u4f5c\u8bcd")) || t.contains(QStringLiteral("\u4f5c\u66f2")) || t.contains(QStringLiteral("\u7f16\u66f2")) || t.startsWith(QStringLiteral("\u8bcd\uff1a")) || t.startsWith(QStringLiteral("\u66f2\uff1a")) || t.contains(QStringLiteral("\u5236\u4f5c\u4eba")));
		if (!isMeta)
		{
			metaOnly = false;
			break;
		}
	}
	if (metaOnly)
		return false;
	outLyric = lyric;
	return true;
}

void MusicController::saveLyricToCache(const QString &key, const Lyric &lyric)
{
	QJsonArray arr;
	for (const LyricLine &ll : lyric.lines)
	{
		QJsonObject o;
		o.insert(QStringLiteral("t"), static_cast<double>(ll.timeMs));
		o.insert(QStringLiteral("x"), ll.text);
		arr.append(o);
	}
	QJsonObject root;
	root.insert(QStringLiteral("lines"), arr);
	QJsonDocument doc(root);
	lyricCache.put(key, doc.toJson(QJsonDocument::Compact));
}

void MusicController::requestLyric(const QString &providerId, const QString &songId)
{
	// 记录本次请求序号，任何晚到的旧回调都必须被忽略
	quint64 requestId = ++m_lyricRequestId;
	if (lyricToken)
		lyricToken->cancel();
	QString key = providerId + QStringLiteral(":") + songId;
	Lyric cached;
	if (lyricFromCache(key, cached))
	{
		if (requestId == m_lyricRequestId)
		{
			m_lyricModel.setLyric(cached);
			updateCurrentLyricIndexByPosition(m_player.position());
		}
	}
	lyricToken = providerManager.lyric(songId, [this, key, requestId](Result<Lyric> result) {
		if (requestId != m_lyricRequestId)
			return;
		if (!result.ok)
		{
			clearLyric();
			return;
		}
		saveLyricToCache(key, result.value);
		m_lyricModel.setLyric(result.value);
		updateCurrentLyricIndexByPosition(m_player.position());
	}, QStringList() << providerId);
}



void MusicController::requestCover(const QUrl &coverUrl)
{
	// 记录本次请求序号，避免快速切歌时旧封面覆盖新封面
	quint64 requestId = ++m_coverRequestId;
	if (!coverUrl.isValid() || coverUrl.isEmpty())
	{
		setCoverSource({});
		return;
	}
	QString key = coverUrl.toString();
	QStringList exts{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("gif"), QStringLiteral("bmp")};
	if (imageCache.containsAny(key, exts))
	{
		if (requestId != m_coverRequestId)
			return;
		QUrl u = imageCache.resolveExistingFileUrlForKey(key, exts);
		setCoverSource(u);
		return;
	}
	if (coverToken)
		coverToken->cancel();
	coverToken = providerManager.cover(coverUrl, [this, key, requestId, coverUrl](Result<QByteArray> result) {
		if (requestId != m_coverRequestId)
			return;
		if (!result.ok)
		{
			Logger::warning(QStringLiteral("Cover fetch failed: %1").arg(coverUrl.toString()));
			return;
		}
		auto sniffExt = [](const QByteArray &data) -> QString {
			if (data.size() >= 8)
			{
				const uchar *p = reinterpret_cast<const uchar *>(data.constData());
				if (p[0] == 0x89 && p[1] == 0x50 && p[2] == 0x4E && p[3] == 0x47 && p[4] == 0x0D && p[5] == 0x0A && p[6] == 0x1A && p[7] == 0x0A)
					return QStringLiteral("png");
			}
			if (data.size() >= 3)
			{
				const uchar *p = reinterpret_cast<const uchar *>(data.constData());
				if (p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF)
					return QStringLiteral("jpg");
			}
			if (data.size() >= 6 && (QByteArray(data.constData(), 6) == QByteArray("GIF87a") || QByteArray(data.constData(), 6) == QByteArray("GIF89a")))
				return QStringLiteral("gif");
			if (data.size() >= 2 && QByteArray(data.constData(), 2) == QByteArray("BM"))
				return QStringLiteral("bmp");
			return QString();
		};
		QString ext = sniffExt(result.value);
		if (ext.isEmpty())
		{
			QBuffer buf;
			buf.setData(result.value);
			buf.open(QIODevice::ReadOnly);
			QImageReader reader(&buf);
			QImage img = reader.read();
			if (!img.isNull())
			{
				QBuffer out;
				out.open(QIODevice::WriteOnly);
				QImageWriter w(&out, "PNG");
				if (w.write(img))
				{
					imageCache.putWithExt(key, out.data(), QStringLiteral("png"));
					QUrl u = imageCache.fileUrlForKeyExt(key, QStringLiteral("png"));
					setCoverSource(u);
					return;
				}
			}
			// 无法解析，直接使用远程地址作为兜底
			Logger::warning(QStringLiteral("Cover decode failed, using remote url: %1").arg(coverUrl.toString()));
			setCoverSource(coverUrl);
			return;
		}
		imageCache.putWithExt(key, result.value, ext);
		QUrl u = imageCache.fileUrlForKeyExt(key, ext);
		setCoverSource(u);
	});
}

void MusicController::setPlaying(bool v)
{
	if (m_playing == v)
		return;
	m_playing = v;
	emit playingChanged();
}

void MusicController::search(const QString &keyword)
{
	if (keyword.trimmed().isEmpty())
		return;

	m_searchKeyword = keyword;
	m_searchOffset = 0;
	m_searchHasMore = true;
	emit searchHasMoreChanged();

	// 记录本次搜索序号，保证只展示最新一次搜索结果
	quint64 requestId = ++m_searchRequestId;
	if (searchToken)
		searchToken->cancel();
	setLoading(true);
	searchToken = providerManager.search(keyword, m_searchLimit, m_searchOffset, [this, requestId](Result<QList<Song>> result) {
		if (requestId != m_searchRequestId)
			return;
		setLoading(false);
		if (!result.ok)
		{
			Logger::warning(QStringLiteral("Search failed: %1 (%2)").arg(result.error.message).arg(result.error.detail));
			emit errorOccurred(result.error.message);
			return;
		}

		if (result.value.size() < m_searchLimit)
		{
			m_searchHasMore = false;
			emit searchHasMoreChanged();
		}

		m_songsModel.setSongs(result.value);
	});
}

void MusicController::loadNextSearchPage()
{
	if (m_loading || !m_searchHasMore || m_searchKeyword.isEmpty())
		return;

	m_searchOffset += m_searchLimit;
	quint64 requestId = m_searchRequestId;

	setLoading(true);
	// 使用 providerManager.search，复用 searchToken (会自动 cancel 上一次，但此时应当是 idle 状态)
	searchToken = providerManager.search(m_searchKeyword, m_searchLimit, m_searchOffset, [this, requestId](Result<QList<Song>> result) {
		if (requestId != m_searchRequestId)
			return;
		setLoading(false);
		if (!result.ok)
		{
			Logger::warning(QStringLiteral("Search page load failed: %1 (%2)").arg(result.error.message).arg(result.error.detail));
			// 加载失败时回退 offset，允许重试
			m_searchOffset -= m_searchLimit;
			return;
		}

		if (result.value.size() < m_searchLimit)
		{
			m_searchHasMore = false;
			emit searchHasMoreChanged();
		}

		m_songsModel.append(result.value);
	});
}

bool MusicController::searchHasMore() const
{
	return m_searchHasMore;
}

void MusicController::playIndex(int index)
{
	if (index < 0 || index >= m_queueModel.rowCount())
		return;
	setCurrentSongIndex(index);
	// 记录本次播放序号，避免旧 playUrl 回调覆盖当前播放
	quint64 requestId = ++m_playRequestId;
	if (playUrlToken)
		playUrlToken->cancel();
	const QList<Song> &songs = m_queueModel.songs();
	if (index < 0 || index >= songs.size())
		return;
	const Song &song = songs.at(index);
	QString songId = song.id;
    m_currentSongId = songId;
	QString providerId = song.providerId;
	QString source = song.source;
	QStringList artistNames;
	for (const Artist &a : song.artists)
		artistNames.append(a.name);
	setCurrentSongTitle(song.name);
	setCurrentSongArtists(artistNames.join(QStringLiteral(" / ")));
	clearLyric();
	setCoverSource({});
	if (song.album.coverUrl.isValid() && !song.album.coverUrl.isEmpty())
	{
		requestCover(song.album.coverUrl);
	}
	else
	{
		if (songDetailToken)
			songDetailToken->cancel();
		QString prefer = providerId;
		if (prefer.isEmpty())
			prefer = source;
		songDetailToken = providerManager.songDetail(songId, [this, requestId](Result<Song> detail) {
			if (requestId != m_playRequestId)
				return;
			if (!detail.ok)
				return;
			if (detail.value.album.coverUrl.isValid() && !detail.value.album.coverUrl.isEmpty())
				requestCover(detail.value.album.coverUrl);
		}, QStringList() << prefer << QStringLiteral("netease"));
	}
	if (providerId == QStringLiteral("netease"))
		requestLyric(providerId, songId);
	else if (source == QStringLiteral("netease"))
		requestLyric(QStringLiteral("netease"), songId);
	setLoading(true);
	QString opaqueSongId = songId;
	if (providerId == QStringLiteral("gdstudio"))
		opaqueSongId = source + QStringLiteral(":") + songId;
	playUrlToken = providerManager.playUrl(opaqueSongId, [this, requestId](Result<PlayUrl> result) {
		if (requestId != m_playRequestId)
			return;
		setLoading(false);
		if (!result.ok)
		{
			Logger::warning(QStringLiteral("PlayUrl failed: %1 (%2)").arg(result.error.message).arg(result.error.detail));
			emit errorOccurred(result.error.message);
			return;
		}
		setCurrentUrl(result.value.url);
		m_player.play();
	}, QStringList() << providerId);
}

void MusicController::playPrev()
{
	playPrevInternal(true);
}

void MusicController::playNext()
{
	playNextInternal(true);
}

void MusicController::cyclePlaybackMode()
{
	setPlaybackMode((m_playbackMode + 1) % 4);
}

void MusicController::loadPlaylist(const QString &playlistId)
{
	QString id = playlistId.trimmed();
	if (id.isEmpty())
		return;

	// 记录本次歌单详情请求序号
	quint64 requestId = ++m_playlistDetailRequestId;
	if (playlistDetailToken)
		playlistDetailToken->cancel();
	m_playlistId = id;
	setPlaylistLoading(true);
	m_requestedPages.clear();
	m_playlistPageTokens.clear();
	m_lastRequestedPage = -1;


	playlistDetailToken = providerManager.playlistDetail(id, [this, requestId](Result<PlaylistMeta> result) {
		if (requestId != m_playlistDetailRequestId)
			return;

		setPlaylistLoading(false);
		if (!result.ok)
		{
			emit errorOccurred(result.error.message);
			return;
		}

		setPlaylistName(result.value.name);
		m_playlistTotal = result.value.trackCount;
		m_playlistModel.setTotalCount(m_playlistTotal);
		requestCover(result.value.coverUrl);
		
		// Load first page
		loadPlaylistPage(0);
	});
}

void MusicController::loadUserPlaylist(const QString &uid)
{
	QString targetUid = uid;
	if (targetUid.isEmpty())
		targetUid = m_userProfile.userId;

	if (targetUid.isEmpty())
	{
		emit errorOccurred(QStringLiteral("User ID is required to load playlist"));
		return;
	}

	if (!neteaseProvider)
	{
		emit errorOccurred(QStringLiteral("Netease provider not available"));
		return;
	}

	if (userPlaylistToken)
	{
		userPlaylistToken->cancel();
		userPlaylistToken.clear();
	}

	m_userPlaylistRequestId++;
	quint64 reqId = m_userPlaylistRequestId;

	userPlaylistToken = neteaseProvider->userPlaylist(targetUid, 1000, 0, [this, reqId, targetUid](Result<QList<PlaylistMeta>> result) {
		if (reqId != m_userPlaylistRequestId)
			return;
		userPlaylistToken.clear();

		if (!result.ok)
		{
			emit errorOccurred(result.error.message);
			return;
		}

		m_userPlaylistModel.setPlaylists(result.value);

		QList<PlaylistMeta> created;
		QList<PlaylistMeta> collected;
		for (const auto &p : result.value) {
			if (p.creatorId == targetUid) {
				created.append(p);
			} else {
				collected.append(p);
			}
		}
		m_createdPlaylistModel.setPlaylists(created);
		m_collectedPlaylistModel.setPlaylists(collected);
	});
}

void MusicController::adjustLyricOffsetMs(qint64 deltaMs)
{
	setLyricOffsetMs(m_lyricOffsetMs + deltaMs);
}

void MusicController::onPlaylistRowRequested(int index)
{
	int pageSize = m_playlistPageSize;
	int page = index / pageSize;
    
    // Update last requested page for cleanup logic
    m_lastRequestedPage = page;
	
	if (!m_playlistModel.isLoaded(page * pageSize) && !m_requestedPages.contains(page))
	{
		loadPlaylistPage(page);
	}

	// Prefetch next 2 pages
	for (int i = 1; i <= 2; ++i) {
		int nextPage = page + i;
		if (nextPage * pageSize < m_playlistTotal)
		{
			if (!m_playlistModel.isLoaded(nextPage * pageSize) && !m_requestedPages.contains(nextPage))
			{
				loadPlaylistPage(nextPage);
			}
		}
	}
}

void MusicController::loadPlaylistPage(int page)
{
	if (m_playlistId.isEmpty()) return;
	
    // Check if this page is already loading
    if (m_playlistPageTokens.contains(page)) return;
	
	m_requestedPages.insert(page);
	
	int pageSize = m_playlistPageSize;
	int limit = pageSize;
	int offset = page * pageSize;
	
    // No global request ID check needed for per-page tokens
    // Use isFinished to handle potentially synchronous callbacks
    auto isFinished = QSharedPointer<bool>::create(false);
	
	auto token = providerManager.playlistTracks(m_playlistId, limit, offset, [this, page, offset, isFinished](Result<PlaylistTracksPage> result) {
        *isFinished = true;
        // Remove token first
        m_playlistPageTokens.remove(page);
		m_requestedPages.remove(page);
		
		if (!result.ok) {
			emit errorOccurred(result.error.message);
			return;
		}
		
		m_playlistModel.updateRange(offset, result.value.songs);

		// Unload pages outside of window centered on m_lastRequestedPage
		// Buffer strategy: Keep 3 pages before and after current page (User requested max 3 pages buffer)
		// This balances memory usage with smooth scrolling/image retention.
		int centerPage = (m_lastRequestedPage >= 0) ? m_lastRequestedPage : page;

		int pageSize = m_playlistPageSize;
		int buffer = pageSize * 3; 

		int keepStart = qMax(0, centerPage * pageSize - buffer);
		int keepEnd = (centerPage + 1) * pageSize + buffer;

		// Clear before keepStart
		if (keepStart > 0) {
			m_playlistModel.clearRange(0, keepStart);
		}
		// Clear after keepEnd
		if (keepEnd < m_playlistTotal) {
			m_playlistModel.clearRange(keepEnd, m_playlistTotal - keepEnd);
		}
	});
    
    if (token && !*isFinished) {
        m_playlistPageTokens.insert(page, token);
    }
}
void MusicController::importPlaylistToQueue(const QString &playlistId, bool clearFirst, const QString &playSongId, bool preventReplay)
{
    QString targetId = playlistId.isEmpty() ? m_playlistId : playlistId;
    if (targetId.isEmpty()) return;

    if (importToken)
        importToken->cancel();

    // Import first 1000 tracks (sufficient for most)
    // To support more, we'd need recursive fetching
    int limit = 1000; 
    int offset = 0;

    importToken = providerManager.playlistTracks(targetId, limit, offset, [this, clearFirst, playSongId, preventReplay](Result<PlaylistTracksPage> result) {
        importToken.clear();
        if (!result.ok) {
            emit errorOccurred("Import failed: " + result.error.message);
            return;
        }

        QList<Song> songs = result.value.songs;
        if (songs.isEmpty()) {
             emit errorOccurred("Playlist is empty");
             return;
        }

        auto songKey = [](const Song &x) {
            QString p = x.providerId.isEmpty() ? x.source : x.providerId;
            return p + QStringLiteral(":") + x.id;
        };
        
        QList<Song> queue;
        QSet<QString> existingKeys;

        if (!clearFirst) {
            queue = m_queueModel.songs();
            for (const Song &s : queue) {
                existingKeys.insert(songKey(s));
            }
        }
        
        int added = 0;
        for (const Song &s : songs) {
            if (s.id.isEmpty()) continue;
            QString key = songKey(s);
            if (!existingKeys.contains(key)) {
                queue.append(s);
                existingKeys.insert(key);
                added++;
            }
        }
        
        if (added > 0 || clearFirst) {
            m_queueModel.setSongs(queue);
            saveQueueToSettings();
        }

        if (!playSongId.isEmpty()) {
            for (int i = 0; i < queue.size(); ++i) {
                if (queue.at(i).id == playSongId) {
                    if (preventReplay && m_currentSongId == playSongId) {
                         setCurrentSongIndex(i);
                         saveQueueToSettings();
                         return;
                    }
                    playIndex(i);
                    return;
                }
            }
        }
    });
}

void MusicController::playPlaylistTrack(int index)
{
	if (index < 0 || index >= m_playlistModel.songs().size())
		return;

    const Song &s = m_playlistModel.songs().at(index);
    // 立即播放当前点击的歌曲，无需等待整个歌单加载
    m_queueModel.setSongs({s});
    playIndex(0);
    // 后台加载完整歌单并更新队列，同时避免重新触发播放
    importPlaylistToQueue(QString(), true, s.id, true);
}

void MusicController::seek(qint64 positionMs)
{
	if (positionMs < 0)
		positionMs = 0;
	qint64 dur = m_player.duration();
	if (dur > 0 && positionMs > dur)
		positionMs = dur;
	m_player.setPosition(positionMs);
}

void MusicController::pause()
{
	m_player.pause();
}

void MusicController::resume()
{
	m_player.play();
}

void MusicController::handleMediaFinished()
{
	if (m_currentSongIndex < 0)
		return;
	if (m_playbackMode == static_cast<int>(LoopOne))
	{
		m_player.setPosition(0);
		m_player.play();
		return;
	}
	playNextInternal(false);
}

void MusicController::playNextInternal(bool fromUser)
{
	int count = m_queueModel.rowCount();
	if (count <= 0)
		return;
	if (m_currentSongIndex < 0)
	{
		playIndex(0);
		return;
	}

	if (m_playbackMode == static_cast<int>(Random))
	{
		if (count == 1)
		{
			playIndex(0);
			return;
		}
		int nextIndex = m_currentSongIndex;
		for (int i = 0; i < 6 && nextIndex == m_currentSongIndex; ++i)
			nextIndex = QRandomGenerator::global()->bounded(count);
		if (nextIndex == m_currentSongIndex)
			nextIndex = (m_currentSongIndex + 1) % count;
		playIndex(nextIndex);
		return;
	}

	if (m_playbackMode == static_cast<int>(LoopAll))
	{
		playIndex((m_currentSongIndex + 1) % count);
		return;
	}

	if (m_currentSongIndex + 1 >= count)
	{
		if (!fromUser)
			return;
		return;
	}
	playIndex(m_currentSongIndex + 1);
}

void MusicController::playPrevInternal(bool fromUser)
{
	Q_UNUSED(fromUser);
	int count = m_queueModel.rowCount();
	if (count <= 0)
		return;
	if (m_currentSongIndex < 0)
	{
		playIndex(0);
		return;
	}

	if (m_playbackMode == static_cast<int>(Random))
	{
		if (count == 1)
		{
			playIndex(0);
			return;
		}
		int prevIndex = m_currentSongIndex;
		for (int i = 0; i < 6 && prevIndex == m_currentSongIndex; ++i)
			prevIndex = QRandomGenerator::global()->bounded(count);
		if (prevIndex == m_currentSongIndex)
			prevIndex = (m_currentSongIndex - 1 + count) % count;
		playIndex(prevIndex);
		return;
	}

	if (m_playbackMode == static_cast<int>(LoopAll))
	{
		playIndex((m_currentSongIndex - 1 + count) % count);
		return;
	}

	if (m_currentSongIndex <= 0)
		return;
	playIndex(m_currentSongIndex - 1);
}

void MusicController::stop()
{
	m_player.stop();
}

void MusicController::queuePlayFromSearchIndex(int index)
{
	if (index < 0 || index >= m_songsModel.rowCount())
		return;
	const QList<Song> &src = m_songsModel.songs();
	Song s = src.at(index);
    auto songKey = [](const Song &x) {
        QString p = x.providerId.isEmpty() ? x.source : x.providerId;
        return p + QStringLiteral(":") + x.id;
    };
    QString key = songKey(s);
    int existing = -1;
    const QList<Song> &queue = m_queueModel.songs();
    for (int i = 0; i < queue.size(); ++i) {
        if (songKey(queue.at(i)) == key) { existing = i; break; }
    }
    if (existing >= 0) {
        playIndex(existing);
        return;
    }
    m_queueModel.append(s);
    saveQueueToSettings();
    playIndex(m_queueModel.rowCount() - 1);
}

void MusicController::queueAddFromSearchIndex(int index, bool next)
{
	if (index < 0 || index >= m_songsModel.rowCount())
		return;
	const QList<Song> &src = m_songsModel.songs();
	Song s = src.at(index);
    auto songKey = [](const Song &x) {
        QString p = x.providerId.isEmpty() ? x.source : x.providerId;
        return p + QStringLiteral(":") + x.id;
    };
    QString key = songKey(s);
    int existing = -1;
    const QList<Song> &queue = m_queueModel.songs();
    for (int i = 0; i < queue.size(); ++i) {
        if (songKey(queue.at(i)) == key) { existing = i; break; }
    }
    if (existing >= 0) {
        playIndex(existing);
        return;
    }
    int insertPos = m_queueModel.rowCount();
	if (next && m_currentSongIndex >= 0)
		insertPos = qMin(m_currentSongIndex + 1, m_queueModel.rowCount());
	m_queueModel.insert(insertPos, s);
	saveQueueToSettings();
}

void MusicController::queueAddFromPlaylistIndex(int index, bool next)
{
	if (index < 0 || index >= m_playlistModel.songs().size())
		return;
	const QList<Song> &src = m_playlistModel.songs();
	Song s = src.at(index);
    auto songKey = [](const Song &x) {
        QString p = x.providerId.isEmpty() ? x.source : x.providerId;
        return p + QStringLiteral(":") + x.id;
    };
    QString key = songKey(s);
    int existing = -1;
    const QList<Song> &queue = m_queueModel.songs();
    for (int i = 0; i < queue.size(); ++i) {
        if (songKey(queue.at(i)) == key) { existing = i; break; }
    }
    if (existing >= 0) {
        playIndex(existing);
        return;
    }
    int insertPos = m_queueModel.rowCount();
	if (next && m_currentSongIndex >= 0)
		insertPos = qMin(m_currentSongIndex + 1, m_queueModel.rowCount());
	m_queueModel.insert(insertPos, s);
	saveQueueToSettings();
}

void MusicController::queueRemoveAt(int index)
{
	m_queueModel.removeAt(index);
	saveQueueToSettings();
}

void MusicController::queueClear()
{
	stop();
	m_queueModel.clear();
	setCurrentSongIndex(-1);
	
	// Reset current song info
	setCurrentSongTitle(QString());
	setCurrentSongArtists(QString());
	setCoverSource(QUrl());
	clearLyric();
	m_currentSongId.clear();
	setCurrentUrl(QUrl());
	setPositionMs(0);
	setDurationMs(0);

	saveQueueToSettings();
}

void MusicController::saveQueueToSettings()
{
	QJsonArray arr;
	for (const Song &s : m_queueModel.songs())
	{
		QJsonObject o;
		o.insert(QStringLiteral("providerId"), s.providerId);
		o.insert(QStringLiteral("source"), s.source);
		o.insert(QStringLiteral("id"), s.id);
		o.insert(QStringLiteral("name"), s.name);
		QJsonArray artistArr;
		for (const Artist &a : s.artists)
		{
			QJsonObject ao;
			ao.insert(QStringLiteral("id"), a.id);
			ao.insert(QStringLiteral("name"), a.name);
			artistArr.append(ao);
		}
		o.insert(QStringLiteral("artists"), artistArr);
		QJsonObject albumObj;
		albumObj.insert(QStringLiteral("id"), s.album.id);
		albumObj.insert(QStringLiteral("name"), s.album.name);
		albumObj.insert(QStringLiteral("coverUrl"), s.album.coverUrl.toString());
		o.insert(QStringLiteral("album"), albumObj);
		o.insert(QStringLiteral("durationMs"), static_cast<double>(s.durationMs));
		arr.append(o);
	}
	QJsonObject root;
	root.insert(QStringLiteral("items"), arr);
	QJsonDocument doc(root);
	QSettings settings;
	settings.beginGroup(QStringLiteral("queue"));
	settings.setValue(QStringLiteral("items"), QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
	settings.endGroup();
}

void MusicController::loadQueueFromSettings()
{
	QSettings settings;
	settings.beginGroup(QStringLiteral("queue"));
	QString json = settings.value(QStringLiteral("items"), QString()).toString();
	settings.endGroup();
	if (json.trimmed().isEmpty())
		return;
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return;
	QJsonArray arr = doc.object().value(QStringLiteral("items")).toArray();
	QList<Song> restored;
	for (const QJsonValue &v : arr)
	{
		QJsonObject o = v.toObject();
		Song s;
		s.providerId = o.value(QStringLiteral("providerId")).toString();
		s.source = o.value(QStringLiteral("source")).toString();
		s.id = o.value(QStringLiteral("id")).toString();
		s.name = o.value(QStringLiteral("name")).toString();
		s.durationMs = static_cast<qint64>(o.value(QStringLiteral("durationMs")).toDouble());
		QJsonArray artistArr = o.value(QStringLiteral("artists")).toArray();
		for (const QJsonValue &av : artistArr)
		{
			QJsonObject ao = av.toObject();
			Artist a;
			a.id = ao.value(QStringLiteral("id")).toString();
			a.name = ao.value(QStringLiteral("name")).toString();
			s.artists.append(a);
		}
		QJsonObject albumObj = o.value(QStringLiteral("album")).toObject();
		s.album.id = albumObj.value(QStringLiteral("id")).toString();
		s.album.name = albumObj.value(QStringLiteral("name")).toString();
		s.album.coverUrl = QUrl(albumObj.value(QStringLiteral("coverUrl")).toString());
		restored.append(s);
	}
	m_queueModel.setSongs(restored);
}

bool MusicController::loggedIn() const
{
	return !m_userProfile.userId.isEmpty();
}

QString MusicController::userId() const
{
	return m_userProfile.userId;
}

QString MusicController::nickname() const
{
	return m_userProfile.nickname;
}

QUrl MusicController::avatarUrl() const
{
	return m_userProfile.avatarUrl;
}

QString MusicController::signature() const
{
	return m_userProfile.signature;
}

int MusicController::vipType() const
{
	return m_userProfile.vipType;
}

void MusicController::loginQrKey()
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginQrKey([this](Result<LoginQrKey> result) {
		if (!result.ok)
		{
			emit loginFailed(result.error.message);
			return;
		}
		emit loginQrKeyReceived(result.value.unikey);
	});
}

void MusicController::loginQrCreate(const QString &key)
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginQrCreate(key, [this](Result<LoginQrCreate> result) {
		if (!result.ok)
		{
			emit loginFailed(result.error.message);
			return;
		}
		emit loginQrCreateReceived(result.value.qrImg, result.value.qrUrl);
	});
}

void MusicController::loginQrCheck(const QString &key)
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginQrCheck(key, [this](Result<LoginQrCheck> result) {
		if (!result.ok)
		{
			emit loginFailed(result.error.message);
			return;
		}
		emit loginQrCheckReceived(result.value.code, result.value.message, result.value.cookie);
		if (result.value.code == 803)
		{
			neteaseProvider->setCookie(result.value.cookie);
			QSettings settings;
			settings.beginGroup(QStringLiteral("auth"));
			settings.setValue(QStringLiteral("cookie"), result.value.cookie);
			settings.endGroup();
			QTimer::singleShot(0, this, [this] { checkLoginStatus(); });
		}
	});
}

void MusicController::loginCellphone(const QString &phone, const QString &password, const QString &countryCode)
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginCellphone(phone, password, countryCode, [this](Result<UserProfile> result) {
		if (!result.ok)
		{
			emit loginFailed(result.error.message);
			return;
		}
		m_userProfile = result.value;
		if (!m_userProfile.cookie.isEmpty())
		{
			neteaseProvider->setCookie(m_userProfile.cookie);
			QSettings settings;
			settings.beginGroup(QStringLiteral("auth"));
			settings.setValue(QStringLiteral("cookie"), m_userProfile.cookie);
			settings.endGroup();
		}
		emit loggedInChanged();
		emit userProfileChanged();
		emit loginSuccess(m_userProfile.userId);
	});
}

void MusicController::loginEmail(const QString &email, const QString &password)
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginEmail(email, password, [this](Result<UserProfile> result) {
		if (!result.ok)
		{
			emit loginFailed(result.error.message);
			return;
		}
		m_userProfile = result.value;
		if (!m_userProfile.cookie.isEmpty())
		{
			neteaseProvider->setCookie(m_userProfile.cookie);
			QSettings settings;
			settings.beginGroup(QStringLiteral("auth"));
			settings.setValue(QStringLiteral("cookie"), m_userProfile.cookie);
			settings.endGroup();
		}
		emit loggedInChanged();
		emit userProfileChanged();
		emit loginSuccess(m_userProfile.userId);
	});
}

void MusicController::loginRefresh()
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginRefresh([this](Result<UserProfile> result) {
		if (!result.ok)
		{
			emit loginFailed(result.error.message);
			return;
		}
		m_userProfile = result.value;
		emit loggedInChanged();
		emit userProfileChanged();
	});
}

void MusicController::logout()
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->logout([this](Result<bool> result) {
		if (!result.ok)
		{
			// Even if server logout fails, we clear local session
			Logger::warning("Logout failed on server: " + result.error.message);
		}

		m_userProfile = UserProfile();
		neteaseProvider->setCookie(QString());
		QSettings settings;
		settings.beginGroup(QStringLiteral("auth"));
		settings.remove(QStringLiteral("cookie"));
		settings.endGroup();

		// Clear user data
		m_userPlaylistModel.clear();
		// Also clear current playlist context as it might be user-specific
		m_playlistModel.clear();
		setPlaylistName(QString());
		m_playlistId.clear();
		setPlaylistHasMore(false);

		emit loggedInChanged();
		emit userProfileChanged();
	});
}

void MusicController::loginCellphoneCaptcha(const QString &phone, const QString &captcha, const QString &countryCode)
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginCellphoneCaptcha(phone, captcha, countryCode, [this](Result<UserProfile> result) {
		if (!result.ok)
		{
			emit loginFailed(result.error.message);
			return;
		}
		m_userProfile = result.value;
		if (!m_userProfile.cookie.isEmpty())
		{
			neteaseProvider->setCookie(m_userProfile.cookie);
			QSettings settings;
			settings.beginGroup(QStringLiteral("auth"));
			settings.setValue(QStringLiteral("cookie"), m_userProfile.cookie);
			settings.endGroup();
		}
		emit loggedInChanged();
		emit userProfileChanged();
		emit loginSuccess(m_userProfile.userId);
	});
}

void MusicController::captchaSent(const QString &phone, const QString &countryCode)
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->captchaSent(phone, countryCode, [this](Result<bool> result) {
		if (!result.ok)
		{
			emit captchaSentReceived(false, result.error.message);
			return;
		}
		emit captchaSentReceived(true, QString());
	});
}

void MusicController::captchaVerify(const QString &phone, const QString &captcha, const QString &countryCode)
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->captchaVerify(phone, captcha, countryCode, [this](Result<bool> result) {
		if (!result.ok)
		{
			emit captchaVerifyReceived(false, result.error.message);
			return;
		}
		emit captchaVerifyReceived(true, QString());
	});
}

void MusicController::playlistRemoveAt(int index)
{
	m_playlistModel.removeAt(index);
}

void MusicController::checkLoginStatus()
{
	if (loginToken)
		loginToken->cancel();
	loginToken = neteaseProvider->loginStatus([this](Result<UserProfile> result) {
		if (!result.ok)
		{
			m_userProfile = UserProfile();
			
			// Clear user data
			m_userPlaylistModel.clear();
			// Also clear current playlist context as it might be user-specific
			m_playlistModel.clear();
			setPlaylistName(QString());
			m_playlistId.clear();
			setPlaylistHasMore(false);

			emit loggedInChanged();
			emit userProfileChanged();
			return;
		}
		m_userProfile = result.value;
		if (!m_userProfile.cookie.isEmpty())
		{
			neteaseProvider->setCookie(m_userProfile.cookie);
			QSettings settings;
			settings.beginGroup(QStringLiteral("auth"));
			settings.setValue(QStringLiteral("cookie"), m_userProfile.cookie);
			settings.endGroup();
		}
		emit loggedInChanged();
		emit userProfileChanged();
		emit loginSuccess(m_userProfile.userId);
	});
}

}
