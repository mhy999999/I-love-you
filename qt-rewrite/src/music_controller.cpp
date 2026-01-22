// MusicController 实现：打通搜索结果到 QtMultimedia 播放
#include "music_controller.h"

#include <QCoreApplication>

namespace App
{

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

	QUrl apiBase(QStringLiteral("http://127.0.0.1:3000"));
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
	if (searchToken)
		searchToken->cancel();
	setLoading(true);
	searchToken = providerManager.search(keyword, 30, [this](Result<QList<Song>> result) {
		setLoading(false);
		if (!result.ok)
		{
			emit errorOccurred(result.error.message);
			return;
		}
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
	setLoading(true);
	playUrlToken = providerManager.playUrl(songId, [this](Result<PlayUrl> result) {
		setLoading(false);
		if (!result.ok)
		{
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

