// MusicController：封装搜索与播放逻辑，供 QML 调用
#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QUrl>
#include <QSet>
#include <QMap>

#include "core_types.h"
#include "disk_cache.h"
#include "http_client.h"
#include "lyric_list_model.h"
#include "playlist_list_model.h"
#include "gdstudio_provider.h"
#include "netease_provider.h"
#include "provider_manager.h"
#include "song_list_model.h"

namespace App
{

class MusicController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY playbackModeChanged)
	Q_PROPERTY(SongListModel *songsModel READ songsModel CONSTANT)
	Q_PROPERTY(LyricListModel *lyricModel READ lyricModel CONSTANT)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
	Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
	Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
	Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionMsChanged)
	Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationMsChanged)
	Q_PROPERTY(int currentSongIndex READ currentSongIndex NOTIFY currentSongIndexChanged)
	Q_PROPERTY(int currentLyricIndex READ currentLyricIndex NOTIFY currentLyricIndexChanged)
	Q_PROPERTY(qint64 lyricOffsetMs READ lyricOffsetMs WRITE setLyricOffsetMs NOTIFY lyricOffsetMsChanged)
	// 便于 QML 计算当前歌词渐进高亮的时间范围
	Q_PROPERTY(qint64 currentLyricStartMs READ currentLyricStartMs NOTIFY currentLyricIndexChanged)
	Q_PROPERTY(qint64 currentLyricNextMs READ currentLyricNextMs NOTIFY currentLyricIndexChanged)
	Q_PROPERTY(QUrl coverSource READ coverSource NOTIFY coverSourceChanged)
	Q_PROPERTY(SongListModel *playlistModel READ playlistModel CONSTANT)
	Q_PROPERTY(SongListModel *queueModel READ queueModel CONSTANT)
	Q_PROPERTY(PlaylistListModel *userPlaylistModel READ userPlaylistModel CONSTANT)
	Q_PROPERTY(PlaylistListModel *createdPlaylistModel READ createdPlaylistModel CONSTANT)
	Q_PROPERTY(PlaylistListModel *collectedPlaylistModel READ collectedPlaylistModel CONSTANT)
	Q_PROPERTY(bool playlistLoading READ playlistLoading NOTIFY playlistLoadingChanged)
	Q_PROPERTY(QString playlistName READ playlistName NOTIFY playlistNameChanged)
	Q_PROPERTY(bool playlistHasMore READ playlistHasMore NOTIFY playlistHasMoreChanged)
	Q_PROPERTY(QString currentSongTitle READ currentSongTitle NOTIFY currentSongTitleChanged)
	Q_PROPERTY(QString currentSongArtists READ currentSongArtists NOTIFY currentSongArtistsChanged)
	Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
	Q_PROPERTY(QString userId READ userId NOTIFY userProfileChanged)
	Q_PROPERTY(QString nickname READ nickname NOTIFY userProfileChanged)
	Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY userProfileChanged)
	Q_PROPERTY(QString signature READ signature NOTIFY userProfileChanged)
	Q_PROPERTY(int vipType READ vipType NOTIFY userProfileChanged)
    Q_PROPERTY(int playlistPageSize READ playlistPageSize WRITE setPlaylistPageSize NOTIFY playlistPageSizeChanged)
	Q_PROPERTY(bool searchHasMore READ searchHasMore NOTIFY searchHasMoreChanged)

public:
	enum PlaybackMode
	{
		Sequence = 0,
		Random = 1,
		LoopAll = 2,
		LoopOne = 3,
	};
	Q_ENUM(PlaybackMode)

	explicit MusicController(QObject *parent = nullptr);
	~MusicController();

	int playbackMode() const;
	void setPlaybackMode(int mode);
    
    int playlistPageSize() const;

	SongListModel *songsModel();
	LyricListModel *lyricModel();
	bool loading() const;
	QUrl currentUrl() const;
	bool playing() const;
	int volume() const;
	void setVolume(int v);
	qint64 positionMs() const;
	qint64 durationMs() const;
	int currentSongIndex() const;
	int currentLyricIndex() const;
	qint64 lyricOffsetMs() const;
	void setLyricOffsetMs(qint64 v);
	qint64 currentLyricStartMs() const;
	qint64 currentLyricNextMs() const;
	QUrl coverSource() const;
	SongListModel *playlistModel();
	SongListModel *queueModel() const;
	PlaylistListModel *userPlaylistModel();
	PlaylistListModel *createdPlaylistModel();
	PlaylistListModel *collectedPlaylistModel();
	bool playlistLoading() const;
	QString playlistName() const;
	bool playlistHasMore() const;
	QString currentSongTitle() const;
	QString currentSongArtists() const;
	bool loggedIn() const;
	QString userId() const;
	QString nickname() const;
	QUrl avatarUrl() const;
	QString signature() const;
	int vipType() const;

	Q_INVOKABLE void search(const QString &keyword);
	Q_INVOKABLE void playIndex(int index);
	Q_INVOKABLE void playPrev();
	Q_INVOKABLE void playNext();
	Q_INVOKABLE void cyclePlaybackMode();
	Q_INVOKABLE void seek(qint64 positionMs);
	Q_INVOKABLE void pause();
	Q_INVOKABLE void resume();
	Q_INVOKABLE void stop();
	Q_INVOKABLE void adjustLyricOffsetMs(qint64 deltaMs);
	Q_INVOKABLE void loadPlaylist(const QString &playlistId);
	Q_INVOKABLE void importPlaylistToQueue(const QString &playlistId = QString(), bool clearFirst = false, const QString &playSongId = QString(), bool preventReplay = false);
	Q_INVOKABLE void playPlaylistTrack(int index);
	Q_INVOKABLE void queuePlayFromSearchIndex(int index);
	Q_INVOKABLE void queueAddFromSearchIndex(int index, bool next);
	Q_INVOKABLE void queueAddFromPlaylistIndex(int index, bool next);
	Q_INVOKABLE void queueRemoveAt(int index);
	Q_INVOKABLE void queueClear();
	Q_INVOKABLE void loginQrKey();
	Q_INVOKABLE void loginQrCreate(const QString &key);
	Q_INVOKABLE void loginQrCheck(const QString &key);
	Q_INVOKABLE void loginCellphone(const QString &phone, const QString &password, const QString &countryCode = QString());
	Q_INVOKABLE void loginCellphoneCaptcha(const QString &phone, const QString &captcha, const QString &countryCode = QString());
	Q_INVOKABLE void captchaSent(const QString &phone, const QString &countryCode = QString());
	Q_INVOKABLE void captchaVerify(const QString &phone, const QString &captcha, const QString &countryCode = QString());
	Q_INVOKABLE void loginEmail(const QString &email, const QString &password);
	Q_INVOKABLE void loginRefresh();
	Q_INVOKABLE void logout();
	Q_INVOKABLE void checkLoginStatus();
	Q_INVOKABLE void playlistRemoveAt(int index);
	Q_INVOKABLE void loadUserPlaylist(const QString &uid = QString());
	Q_INVOKABLE void onPlaylistRowRequested(int index);
	Q_INVOKABLE void loadNextSearchPage();
	bool searchHasMore() const;

signals:
	void loadingChanged();
	void currentUrlChanged();
	void playingChanged();
	void volumeChanged();
	void positionMsChanged();
	void durationMsChanged();
	void currentSongIndexChanged();
	void currentLyricIndexChanged();
	void lyricOffsetMsChanged();
	void coverSourceChanged();
	void playlistLoadingChanged();
	void playlistNameChanged();
	void playlistHasMoreChanged();
	void errorOccurred(const QString &message);
	void currentSongTitleChanged();
	void currentSongArtistsChanged();
	void playbackModeChanged();
    void playlistPageSizeChanged();
	void loggedInChanged();
	void searchHasMoreChanged();
	void userProfileChanged();
	void loginQrKeyReceived(const QString &key);
	void loginQrCreateReceived(const QString &qrImg, const QString &qrUrl);
	void loginQrCheckReceived(int code, const QString &message, const QString &cookie);
	void loginSuccess(const QString &userId);
	void loginFailed(const QString &message);
	void captchaSentReceived(bool success, const QString &message);
	void captchaVerifyReceived(bool success, const QString &message);

private:
	HttpClient httpClient;
	ProviderManager providerManager;
	GdStudioProvider *gdStudioProvider = nullptr;
	NeteaseProvider *neteaseProvider = nullptr;
	SongListModel m_songsModel;
	LyricListModel m_lyricModel;
	SongListModel m_playlistModel;
	SongListModel m_queueModel;
	PlaylistListModel m_userPlaylistModel;
	PlaylistListModel m_createdPlaylistModel;
	PlaylistListModel m_collectedPlaylistModel;
	QMediaPlayer m_player;
	DiskCache imageCache;
	DiskCache lyricCache;
	QSharedPointer<RequestToken> searchToken;
	QSharedPointer<RequestToken> playUrlToken;
	QSharedPointer<RequestToken> lyricToken;
	QSharedPointer<RequestToken> coverToken;
	QSharedPointer<RequestToken> songDetailToken;
	QSharedPointer<RequestToken> playlistDetailToken;
	QSharedPointer<RequestToken> playlistTracksToken;
	QSharedPointer<RequestToken> userPlaylistToken;
	QSharedPointer<RequestToken> loginToken;
	QSharedPointer<RequestToken> importToken;
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
	quint64 m_userPlaylistRequestId = 0;
	qint64 m_positionMs = 0;
	qint64 m_durationMs = 0;
	int m_currentSongIndex = -1;
	int m_currentLyricIndex = -1;
	qint64 m_lyricOffsetMs = 0;
	QUrl m_coverSource;
	bool m_playlistLoading = false;
	QString m_playlistId;
	QString m_playlistName;
	int m_playlistTotal = 0;
	int m_playlistOffset = 0;
	int m_playlistLimit = 50;
    int m_playlistPageSize = 50;
	QString m_currentSongTitle;
	QString m_currentSongId;
	QString m_currentSongArtists;
	int m_playbackMode = Sequence;
	UserProfile m_userProfile;
	
	QString m_searchKeyword;
	int m_searchOffset = 0;
	int m_searchLimit = 30;
	bool m_searchHasMore = false;

	void setLoading(bool v);
	void setCurrentUrl(const QUrl &url);
	void setPlaying(bool v);
	void setPositionMs(qint64 v);
	void setDurationMs(qint64 v);
	void setCurrentSongIndex(int v);
	void setCurrentLyricIndex(int v);
	void setCoverSource(const QUrl &url);
	void setPlaylistLoading(bool v);
	void setPlaylistName(const QString &name);
	void setPlaylistHasMore(bool v);
	Q_INVOKABLE void setPlaylistPageSize(int size);
	void setCurrentSongTitle(const QString &title);
	void setCurrentSongArtists(const QString &artists);
	void updateCurrentLyricIndexByPosition(qint64 posMs);
	void requestLyric(const QString &providerId, const QString &songId);
	void requestCover(const QUrl &coverUrl);
	void clearLyric();
	bool lyricFromCache(const QString &key, Lyric &outLyric);
	void saveLyricToCache(const QString &key, const Lyric &lyric);
	void handleMediaFinished();
	void playNextInternal(bool fromUser);
	void playPrevInternal(bool fromUser);
	bool m_playlistHasMore = false;

	// 队列持久化
	void saveQueueToSettings();
	void loadQueueFromSettings();

    // Lazy loading
    QSet<int> m_requestedPages;
    int m_lastRequestedPage = -1;
    QMap<int, QSharedPointer<RequestToken>> m_playlistPageTokens;
    void loadPlaylistPage(int page);

};

}
