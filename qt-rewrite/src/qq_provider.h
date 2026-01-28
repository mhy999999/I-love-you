#pragma once

#include <QObject>
#include <QSharedPointer>
#include "core_types.h"
#include "http_client.h"
#include "provider.h"

namespace App
{

class QQProvider : public IProvider
{
    Q_OBJECT

public:
    explicit QQProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent = nullptr);

    using UserPlaylistCallback = std::function<void(Result<QList<PlaylistMeta>>)>;

    // IProvider implementation
    QString id() const override;
    QString displayName() const override;
    
    // QQ Music mainly supports playlist import for now
    bool supportsUserPlaylist() const override { return true; }
    
    // Other features not fully implemented yet based on current requirements
    bool supportsSearch() const override { return false; }
    bool supportsSongDetail() const override { return false; }
    bool supportsPlayUrl() const override { return false; }
    
    QSharedPointer<RequestToken> search(const QString &keyword, int limit, int offset, const SearchCallback &callback) override;
    QSharedPointer<RequestToken> songDetail(const QString &songId, const SongDetailCallback &callback) override;
    QSharedPointer<RequestToken> playUrl(const QString &songId, const PlayUrlCallback &callback) override;
    QSharedPointer<RequestToken> lyric(const QString &songId, const LyricCallback &callback) override;
    QSharedPointer<RequestToken> cover(const QUrl &coverUrl, const CoverCallback &callback) override;
    QSharedPointer<RequestToken> playlistDetail(const QString &playlistId, const PlaylistDetailCallback &callback) override;
    QSharedPointer<RequestToken> playlistTracks(const QString &playlistId, int limit, int offset, const PlaylistTracksCallback &callback) override;

    // Specific methods for QQ
    QSharedPointer<RequestToken> userPlaylist(const QString &uid, const UserPlaylistCallback &callback);
    void setCookie(const QString &cookie);

private:
    HttpClient *client;
    QUrl apiBase;
    QString m_cookie;

    QUrl buildUrl(const QString &path, const QList<QPair<QString, QString>> &query) const;
    Result<QList<PlaylistMeta>> parseUserPlaylist(const QByteArray &body, bool isCreated) const;
};

}
