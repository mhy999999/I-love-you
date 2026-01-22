// MusicController 实现：打通搜索结果到 QtMultimedia 播放
#include "music_controller.h"

#include <QCoreApplication>
#include <QAudioOutput>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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
	if (QFileInfo::exists(nodeModulesPath))
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
	, m_player(this)
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
	providerManager.registerProvider(neteaseProvider);
	ProviderManagerConfig cfg;
	cfg.providerOrder = QStringList() << neteaseProvider->id();
	cfg.fallbackEnabled = false;
	providerManager.setConfig(cfg);

	m_player.setAudioOutput(new QAudioOutput(this));
	QObject::connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
		setPlaying(state == QMediaPlayer::PlayingState);
	});
}

SongListModel *MusicController::songsModel()
{
	return &m_songsModel;
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
	QVariantMap songMap = m_songsModel.get(index);
	QString songId = songMap.value(QStringLiteral("songId")).toString();
	Logger::info(QStringLiteral("Play index %1, songId=%2").arg(index).arg(songId));
	setLoading(true);
	playUrlToken = providerManager.playUrl(songId, [this](Result<PlayUrl> result) {
		setLoading(false);
		if (!result.ok)
		{
			Logger::warning(QStringLiteral("PlayUrl failed: %1 (%2)").arg(result.error.message).arg(result.error.detail));
			emit errorOccurred(result.error.message);
			return;
		}
		setCurrentUrl(result.value.url);
		m_player.play();
	});
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
