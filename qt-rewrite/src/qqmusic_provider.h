#pragma once

#include "provider.h"
#include "http_client.h"
#include <QMap>

namespace App
{

class QQMusicProvider : public IProvider
{
	Q_OBJECT

public:
	explicit QQMusicProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent = nullptr);

	QString id() const override { return "qq"; }
	QString displayName() const override { return "QQ Music"; }
	bool supportsLyric() const override { return false; }
	bool supportsCover() const override { return false; }
	bool supportsPlaylistDetail() const override { return false; }
	bool supportsPlaylistTracks() const override { return false; }
    
    QSharedPointer<RequestToken> search(const QString &keyword, int limit, int offset, const SearchCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> searchSuggest(const QString &keyword, const std::function<void(Result<QStringList>)> &callback) override { return nullptr; }
    QSharedPointer<RequestToken> hotSearch(const HotSearchCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> songDetail(const QString &songId, const SongDetailCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> playUrl(const QString &songId, const PlayUrlCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> lyric(const QString &songId, const LyricCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> cover(const QUrl &coverUrl, const CoverCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> playlistDetail(const QString &playlistId, const PlaylistDetailCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> playlistTracks(const QString &playlistId, int limit, int offset, const PlaylistTracksCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> playlistTracksOp(const QString &op, const QString &playlistId, const QString &trackIds, const BoolCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> createPlaylist(const QString &name, const QString &type, bool privacy, const BoolCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> deletePlaylist(const QString &playlistIds, const BoolCallback &callback) override { return nullptr; }
    QSharedPointer<RequestToken> subscribePlaylist(const QString &playlistId, bool subscribe, const BoolCallback &callback) override { return nullptr; }

	using LoginQrKeyCallback = std::function<void(Result<LoginQrKey>)>;
	using LoginQrCreateCallback = std::function<void(Result<LoginQrCreate>)>;
	using LoginQrCheckCallback = std::function<void(Result<LoginQrCheck>)>;

	QSharedPointer<RequestToken> loginQrKey(const LoginQrKeyCallback &callback);
	QSharedPointer<RequestToken> loginQrCreate(const QString &key, const LoginQrCreateCallback &callback);
	QSharedPointer<RequestToken> loginQrCheck(const QString &key, const LoginQrCheckCallback &callback);

	void setCookie(const QString &cookie);
	QString cookie() const;

private:
	HttpClient *client;
	QUrl apiBase;
	QString m_cookie;
    QMap<QString, QString> m_qrImages; // qrsig -> base64 image

	QUrl buildUrl(const QString &path, const QList<QPair<QString, QString>> &query) const;
};

}
