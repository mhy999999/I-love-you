// NeteaseProvider：基于本地 netease-cloud-music-api 的网易云 Provider 实现
#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include "core_types.h"
#include "http_client.h"
#include "provider.h"

namespace App
{

// 网易云 Provider，依赖本地运行的 netease-cloud-music-api 服务
class NeteaseProvider : public IProvider
{
	Q_OBJECT

public:
	explicit NeteaseProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent = nullptr);

	using UserPlaylistCallback = std::function<void(Result<QList<PlaylistMeta>>)>;

	// IProvider 接口实现
	QString id() const override;
	QString displayName() const override;
	bool supportsLyric() const override;
	bool supportsCover() const override;
	bool supportsPlaylistDetail() const override;
	bool supportsPlaylistTracks() const override;
    bool supportsUserPlaylist() const override { return true; }
    bool supportsSearchSuggest() const override { return true; }
    bool supportsHotSearch() const override { return true; }

    QSharedPointer<RequestToken> search(const QString &keyword, int limit, int offset, const SearchCallback &callback) override;
    QSharedPointer<RequestToken> searchSuggest(const QString &keyword, const std::function<void(Result<QStringList>)> &callback) override;
    QSharedPointer<RequestToken> hotSearch(const HotSearchCallback &callback) override;
    QSharedPointer<RequestToken> songDetail(const QString &songId, const SongDetailCallback &callback) override;
	QSharedPointer<RequestToken> playUrl(const QString &songId, const PlayUrlCallback &callback) override;
	QSharedPointer<RequestToken> lyric(const QString &songId, const LyricCallback &callback) override;
	QSharedPointer<RequestToken> cover(const QUrl &coverUrl, const CoverCallback &callback) override;
	QSharedPointer<RequestToken> playlistDetail(const QString &playlistId, const PlaylistDetailCallback &callback) override;
	QSharedPointer<RequestToken> playlistTracks(const QString &playlistId, int limit, int offset, const PlaylistTracksCallback &callback) override;
    
    bool supportsPlaylistTracksOp() const override { return true; }
    QSharedPointer<RequestToken> playlistTracksOp(const QString &op, const QString &playlistId, const QString &trackIds, const BoolCallback &callback) override;

    bool supportsPlaylistCreate() const override { return true; }
    bool supportsPlaylistDelete() const override { return true; }
    bool supportsPlaylistSubscribe() const override { return true; }

    QSharedPointer<RequestToken> createPlaylist(const QString &name, const QString &type, bool privacy, const BoolCallback &callback) override;
    QSharedPointer<RequestToken> deletePlaylist(const QString &playlistIds, const BoolCallback &callback) override;
    QSharedPointer<RequestToken> subscribePlaylist(const QString &playlistId, bool subscribe, const BoolCallback &callback) override;

	QSharedPointer<RequestToken> userPlaylist(const QString &uid, int limit, int offset, const UserPlaylistCallback &callback);

	using LoginQrKeyCallback = std::function<void(Result<LoginQrKey>)>; 
	using LoginQrCreateCallback = std::function<void(Result<LoginQrCreate>)>;
	using LoginQrCheckCallback = std::function<void(Result<LoginQrCheck>)>;
	using LoginCallback = std::function<void(Result<UserProfile>)>;

	void setCookie(const QString &cookie);
	QString cookie() const;

	QSharedPointer<RequestToken> loginQrKey(const LoginQrKeyCallback &callback);
	QSharedPointer<RequestToken> loginQrCreate(const QString &key, const LoginQrCreateCallback &callback);
	QSharedPointer<RequestToken> loginQrCheck(const QString &key, const LoginQrCheckCallback &callback);
	QSharedPointer<RequestToken> loginCellphone(const QString &phone, const QString &password, const QString &countryCode, const LoginCallback &callback);
	QSharedPointer<RequestToken> loginCellphoneCaptcha(const QString &phone, const QString &captcha, const QString &countryCode, const LoginCallback &callback);
	QSharedPointer<RequestToken> captchaSent(const QString &phone, const QString &countryCode, const std::function<void(Result<bool>)> &callback);
	QSharedPointer<RequestToken> captchaVerify(const QString &phone, const QString &captcha, const QString &countryCode, const std::function<void(Result<bool>)> &callback);
	QSharedPointer<RequestToken> loginEmail(const QString &email, const QString &password, const LoginCallback &callback);
	QSharedPointer<RequestToken> loginRefresh(const LoginCallback &callback);
	QSharedPointer<RequestToken> logout(const std::function<void(Result<bool>)> &callback);
	QSharedPointer<RequestToken> loginStatus(const LoginCallback &callback);

private:
	HttpClient *client;
	QUrl apiBase;
	QString m_cookie;

	QUrl buildUrl(const QString &path, const QList<QPair<QString, QString>> &query) const;
	Result<QList<Song>> parseSearchSongs(const QByteArray &body) const;
	Result<Song> parseSongDetail(const QByteArray &body) const;
	Result<PlayUrl> parsePlayUrl(const QByteArray &body) const;
	Result<Lyric> parseLyric(const QByteArray &body) const;
	Result<PlaylistMeta> parsePlaylistDetail(const QByteArray &body) const;
	Result<PlaylistTracksPage> parsePlaylistTracks(const QString &playlistId, int limit, int offset, const QByteArray &body) const;
	Result<LoginQrKey> parseLoginQrKey(const QByteArray &body) const;
	Result<LoginQrCreate> parseLoginQrCreate(const QByteArray &body) const;
	Result<LoginQrCheck> parseLoginQrCheck(const QByteArray &body) const;
	Result<UserProfile> parseLoginResult(const QByteArray &body) const;
	Result<QList<PlaylistMeta>> parseUserPlaylist(const QByteArray &body) const;
    Result<QList<HotSearchItem>> parseHotSearch(const QByteArray &body) const;
};

}

