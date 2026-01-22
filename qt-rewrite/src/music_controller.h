// MusicController：封装搜索与播放逻辑，供 QML 调用
#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QUrl>

#include "core_types.h"
#include "disk_cache.h"
#include "http_client.h"
#include "lyric_list_model.h"
#include "gdstudio_provider.h"
#include "netease_provider.h"
#include "provider_manager.h"
#include "song_list_model.h"

namespace App
{

class MusicController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(SongListModel *songsModel READ songsModel CONSTANT)
	Q_PROPERTY(LyricListModel *lyricModel READ lyricModel CONSTANT)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
	Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
	Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
	Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionMsChanged)
	Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationMsChanged)
	Q_PROPERTY(int currentLyricIndex READ currentLyricIndex NOTIFY currentLyricIndexChanged)
	Q_PROPERTY(QUrl coverSource READ coverSource NOTIFY coverSourceChanged)
	Q_PROPERTY(SongListModel *playlistModel READ playlistModel CONSTANT)
	Q_PROPERTY(bool playlistLoading READ playlistLoading NOTIFY playlistLoadingChanged)
	Q_PROPERTY(QString playlistName READ playlistName NOTIFY playlistNameChanged)
	Q_PROPERTY(bool playlistHasMore READ playlistHasMore NOTIFY playlistHasMoreChanged)

public:
	explicit MusicController(QObject *parent = nullptr);

	SongListModel *songsModel();
	LyricListModel *lyricModel();
	bool loading() const;
	QUrl currentUrl() const;
	bool playing() const;
	int volume() const;
	void setVolume(int v);
	qint64 positionMs() const;
	qint64 durationMs() const;
	int currentLyricIndex() const;
	QUrl coverSource() const;
	SongListModel *playlistModel();
	bool playlistLoading() const;
	QString playlistName() const;
	bool playlistHasMore() const;

	Q_INVOKABLE void search(const QString &keyword);
	Q_INVOKABLE void playIndex(int index);
	Q_INVOKABLE void seek(qint64 positionMs);
	Q_INVOKABLE void pause();
	Q_INVOKABLE void resume();
	Q_INVOKABLE void stop();
	Q_INVOKABLE void loadPlaylist(const QString &playlistId);
	Q_INVOKABLE void loadMorePlaylist();
	Q_INVOKABLE void importPlaylistToQueue();

signals:
	void loadingChanged();
	void currentUrlChanged();
	void playingChanged();
	void volumeChanged();
	void positionMsChanged();
	void durationMsChanged();
	void currentLyricIndexChanged();
	void coverSourceChanged();
	void playlistLoadingChanged();
	void playlistNameChanged();
	void playlistHasMoreChanged();
	void errorOccurred(const QString &message);

private:
	HttpClient httpClient;
	ProviderManager providerManager;
	GdStudioProvider *gdStudioProvider = nullptr;
	NeteaseProvider *neteaseProvider = nullptr;
	SongListModel m_songsModel;
	LyricListModel m_lyricModel;
	SongListModel m_playlistModel;
	QMediaPlayer m_player;
	DiskCache imageCache;
	DiskCache lyricCache;
	QSharedPointer<RequestToken> searchToken;
	QSharedPointer<RequestToken> playUrlToken;
	QSharedPointer<RequestToken> lyricToken;
	QSharedPointer<RequestToken> coverToken;
	QSharedPointer<RequestToken> playlistDetailToken;
	QSharedPointer<RequestToken> playlistTracksToken;
	QProcess *musicApiProcess = nullptr;
	bool m_loading = false;
	QUrl m_currentUrl;
	bool m_playing = false;
	// 递增请求序号：用于在 UI 层丢弃过期回调，规避取消/竞态导致的错歌错图等问题
	quint64 m_searchRequestId = 0;
	quint64 m_playRequestId = 0;
	quint64 m_lyricRequestId = 0;
	quint64 m_coverRequestId = 0;
	quint64 m_playlistDetailRequestId = 0;
	quint64 m_playlistTracksRequestId = 0;
	qint64 m_positionMs = 0;
	qint64 m_durationMs = 0;
	int m_currentLyricIndex = -1;
	QUrl m_coverSource;
	bool m_playlistLoading = false;
	QString m_playlistId;
	QString m_playlistName;
	int m_playlistTotal = 0;
	int m_playlistOffset = 0;
	int m_playlistLimit = 50;

	void setLoading(bool v);
	void setCurrentUrl(const QUrl &url);
	void setPlaying(bool v);
	void setPositionMs(qint64 v);
	void setDurationMs(qint64 v);
	void setCurrentLyricIndex(int v);
	void setCoverSource(const QUrl &url);
	void setPlaylistLoading(bool v);
	void setPlaylistName(const QString &name);
	void setPlaylistHasMore(bool v);
	void updateCurrentLyricIndexByPosition(qint64 posMs);
	void requestLyric(const QString &providerId, const QString &songId);
	void requestCover(const QUrl &coverUrl);
	void clearLyric();
	bool lyricFromCache(const QString &key, Lyric &outLyric);
	void saveLyricToCache(const QString &key, const Lyric &lyric);
	bool m_playlistHasMore = false;
};

}
