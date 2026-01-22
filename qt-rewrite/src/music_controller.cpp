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
#include <QRegularExpression>
#include <QSettings>
#include <QTcpServer>
#include <QThread>
#include <QTimer>

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

	const QStringList roots = {QDir::currentPath(), QCoreApplication::applicationDirPath()};
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
		"api.serveNcmApi({port:%1}).catch(e=>{console.error(e);process.exit(1);});"
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
	int startPort = settings.value(QStringLiteral("musicApiPort"), 30488).toInt();
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
	, m_player(this)
	, imageCache(QStringLiteral("images"), 200LL * 1024 * 1024)
	, lyricCache(QStringLiteral("lyrics"), 20LL * 1024 * 1024)
{
	QMap<QByteArray, QByteArray> headers;
	headers.insert("Accept", "application/json");
	headers.insert("Accept-Encoding", "gzip, deflate");
	httpClient.setDefaultHeaders(headers);
	httpClient.setUserAgent(QByteArray("QtRewrite/1.0 (NeteaseProvider)"));

	QSettings settings;
	settings.beginGroup(QStringLiteral("set"));
	QString apiBaseStr = settings.value(QStringLiteral("musicApiBaseUrl"), QString()).toString().trimmed();
	QString apiDirOverride = settings.value(QStringLiteral("musicApiDir"), QString()).toString().trimmed();
	bool autoStart = settings.value(QStringLiteral("musicApiAutoStart"), true).toBool();
	bool autoInstall = settings.value(QStringLiteral("musicApiAutoInstall"), true).toBool();
	settings.endGroup();

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

	m_player.setAudioOutput(new QAudioOutput(this));
	QObject::connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
		setPlaying(state == QMediaPlayer::PlayingState);
	});
	QObject::connect(&m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
		setPositionMs(pos);
		updateCurrentLyricIndexByPosition(pos);
	});
	QObject::connect(&m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
		setDurationMs(dur);
	});
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

int MusicController::currentLyricIndex() const
{
	return m_currentLyricIndex;
}

QUrl MusicController::coverSource() const
{
	return m_coverSource;
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
	if (m_playlistHasMore == v)
		return;
	m_playlistHasMore = v;
	emit playlistHasMoreChanged();
}

void MusicController::updateCurrentLyricIndexByPosition(qint64 posMs)
{
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
		if (t <= posMs)
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
		lyric.lines.append(ll);
	}
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
	if (lyricToken)
		lyricToken->cancel();
	QString key = providerId + QStringLiteral(":") + songId;
	Lyric cached;
	if (lyricFromCache(key, cached))
	{
		m_lyricModel.setLyric(cached);
		updateCurrentLyricIndexByPosition(m_player.position());
		return;
	}
	lyricToken = providerManager.lyric(songId, [this, key](Result<Lyric> result) {
		if (!result.ok)
		{
			clearLyric();
			return;
		}
		m_lyricModel.setLyric(result.value);
		saveLyricToCache(key, result.value);
		updateCurrentLyricIndexByPosition(m_player.position());
	}, QStringList() << providerId);
}

void MusicController::requestCover(const QUrl &coverUrl)
{
	if (!coverUrl.isValid() || coverUrl.isEmpty())
	{
		setCoverSource({});
		return;
	}
	QString key = coverUrl.toString();
	if (imageCache.contains(key))
	{
		setCoverSource(imageCache.fileUrlForKey(key));
		return;
	}
	if (coverToken)
		coverToken->cancel();
	coverToken = providerManager.cover(coverUrl, [this, key](Result<QByteArray> result) {
		if (!result.ok)
			return;
		imageCache.put(key, result.value);
		setCoverSource(imageCache.fileUrlForKey(key));
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
	Logger::info(QStringLiteral("Search: %1").arg(keyword.trimmed()));
	if (searchToken)
		searchToken->cancel();
	setLoading(true);
	searchToken = providerManager.search(keyword, 30, [this](Result<QList<Song>> result) {
		setLoading(false);
		if (!result.ok)
		{
			Logger::warning(QStringLiteral("Search failed: %1 (%2)").arg(result.error.message).arg(result.error.detail));
			emit errorOccurred(result.error.message);
			return;
		}
		Logger::info(QStringLiteral("Search ok: %1 songs").arg(result.value.size()));
		m_songsModel.setSongs(result.value);
	});
}

void MusicController::playIndex(int index)
{
	if (index < 0 || index >= m_songsModel.rowCount())
		return;
	if (playUrlToken)
		playUrlToken->cancel();
	const QList<Song> &songs = m_songsModel.songs();
	if (index < 0 || index >= songs.size())
		return;
	const Song &song = songs.at(index);
	QString songId = song.id;
	QString providerId = song.providerId;
	QString source = song.source;
	Logger::info(QStringLiteral("Play index %1, provider=%2, songId=%3").arg(index).arg(providerId).arg(songId));
	clearLyric();
	setCoverSource({});
	requestCover(song.album.coverUrl);
	if (providerId == QStringLiteral("netease"))
		requestLyric(providerId, songId);
	else if (source == QStringLiteral("netease"))
		requestLyric(QStringLiteral("netease"), songId);
	setLoading(true);
	QString opaqueSongId = songId;
	if (providerId == QStringLiteral("gdstudio"))
		opaqueSongId = source + QStringLiteral(":") + songId;
	playUrlToken = providerManager.playUrl(opaqueSongId, [this](Result<PlayUrl> result) {
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

void MusicController::loadPlaylist(const QString &playlistId)
{
	QString id = playlistId.trimmed();
	if (id.isEmpty())
		return;
	if (playlistDetailToken)
		playlistDetailToken->cancel();
	if (playlistTracksToken)
		playlistTracksToken->cancel();
	m_playlistId = id;
	m_playlistOffset = 0;
	m_playlistTotal = 0;
	setPlaylistHasMore(false);
	setPlaylistLoading(true);
	m_playlistModel.setSongs({});
	playlistDetailToken = providerManager.playlistDetail(id, [this](Result<PlaylistMeta> result) {
		if (!result.ok)
		{
			setPlaylistLoading(false);
			emit errorOccurred(result.error.message);
			return;
		}
		setPlaylistName(result.value.name);
		m_playlistTotal = result.value.trackCount;
		requestCover(result.value.coverUrl);
		loadMorePlaylist();
	});
}

void MusicController::loadMorePlaylist()
{
	if (m_playlistId.isEmpty())
		return;
	if (playlistTracksToken)
		playlistTracksToken->cancel();
	setPlaylistLoading(true);
	int limit = m_playlistLimit > 0 ? m_playlistLimit : 50;
	int offset = m_playlistOffset;
	playlistTracksToken = providerManager.playlistTracks(m_playlistId, limit, offset, [this, limit, offset](Result<PlaylistTracksPage> result) {
		setPlaylistLoading(false);
		if (!result.ok)
		{
			emit errorOccurred(result.error.message);
			return;
		}
		QList<Song> combined = m_playlistModel.songs();
		combined.append(result.value.songs);
		m_playlistModel.setSongs(combined);
		m_playlistOffset = offset + result.value.songs.size();
		int total = m_playlistTotal > 0 ? m_playlistTotal : result.value.total;
		setPlaylistHasMore(m_playlistOffset < total);
	});
}

void MusicController::importPlaylistToQueue()
{
	m_songsModel.setSongs(m_playlistModel.songs());
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

void MusicController::stop()
{
	m_player.stop();
}

}
