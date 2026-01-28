#include "qq_provider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QDateTime>

#include "json_utils.h"

namespace App
{

namespace
{

Result<QList<PlaylistMeta>> parseUserPlaylistHelper(const QByteArray &body)
{
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        return Result<QList<PlaylistMeta>>::failure(Error{ErrorCategory::Parser, -1, "JSON Parse Error"});
    }

    QJsonArray arr;
    if (doc.isArray())
    {
        arr = doc.array();
    }
    else if (doc.isObject())
    {
        QJsonObject root = doc.object();
        if (root.contains("data"))
        {
            QJsonValue data = root["data"];
            if (data.isArray()) arr = data.toArray();
            else if (data.isObject())
            {
                QJsonObject d = data.toObject();
                if (d.contains("list") && d["list"].isArray()) arr = d["list"].toArray();
            }
        }
        else if (root.contains("list") && root["list"].isArray())
        {
            arr = root["list"].toArray();
        }
    }

    QList<PlaylistMeta> list;
    for (const QJsonValue &v : arr)
    {
        if (!v.isObject()) continue;
        QJsonObject o = v.toObject();

        PlaylistMeta p;
        if (o.contains("tid")) p.id = QString::number(o["tid"].toVariant().toLongLong());
        else if (o.contains("dissid")) p.id = o["dissid"].toVariant().toString();
        else if (o.contains("id")) p.id = o["id"].toVariant().toString();

        if (p.id.isEmpty()) continue;

        if (o.contains("title")) p.name = o["title"].toString();
        else if (o.contains("dissname")) p.name = o["dissname"].toString();
        else if (o.contains("name")) p.name = o["name"].toString();

        QString cover;
        if (o.contains("cover_url")) cover = o["cover_url"].toString();
        else if (o.contains("pic")) cover = o["pic"].toString();
        else if (o.contains("logo")) cover = o["logo"].toString();
        if (!cover.isEmpty()) p.coverUrl = QUrl(cover);

        if (o.contains("song_cnt")) p.trackCount = o["song_cnt"].toInt();
        else if (o.contains("track_count")) p.trackCount = o["track_count"].toInt();
        else if (o.contains("cnt")) p.trackCount = o["cnt"].toInt();

        if (o.contains("creator"))
        {
            QJsonObject c = o["creator"].toObject();
            if (c.contains("qq")) p.creatorId = QString::number(c["qq"].toVariant().toLongLong());
            else if (c.contains("uin")) p.creatorId = QString::number(c["uin"].toVariant().toLongLong());
            if (c.contains("name")) p.creatorName = c["name"].toString();
            else if (c.contains("nick")) p.creatorName = c["nick"].toString();
        }
        else if (o.contains("creator_name"))
        {
            p.creatorName = o["creator_name"].toString();
        }
        else if (o.contains("nickname"))
        {
            p.creatorName = o["nickname"].toString();
        }

        list.append(p);
    }
    return Result<QList<PlaylistMeta>>::success(list);
}

Result<QList<PlaylistMeta>> parseUserDetail(const QByteArray &body)
{
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        return Result<QList<PlaylistMeta>>::failure(Error{ErrorCategory::Parser, -1, "JSON Parse Error"});
    }

    QList<PlaylistMeta> list;
    if (doc.isObject())
    {
        QJsonObject root = doc.object();
        // Check for 301 Login Required
        if (root.contains("code") && root["code"].toInt() == 301)
        {
             return Result<QList<PlaylistMeta>>::failure(Error{ErrorCategory::Network, 301, "Login Required"});
        }

        QJsonObject data;
        if (root.contains("data")) data = root["data"].toObject();
        
        if (data.contains("mymusic"))
        {
             QJsonDocument subDoc(data["mymusic"].toArray());
             auto res = parseUserPlaylistHelper(subDoc.toJson());
             if (res.ok) list.append(res.value);
        }
        if (data.contains("mydiss"))
        {
             QJsonDocument subDoc(data["mydiss"].toArray());
             auto res = parseUserPlaylistHelper(subDoc.toJson());
             if (res.ok) list.append(res.value);
        }
    }
    return Result<QList<PlaylistMeta>>::success(list);
}

}

QQProvider::QQProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent)
    : IProvider(parent)
    , client(httpClient)
    , apiBase(baseUrl)
{
}

QString QQProvider::id() const
{
    return QStringLiteral("qq");
}

QString QQProvider::displayName() const
{
    return QStringLiteral("QQ Music");
}

QSharedPointer<RequestToken> QQProvider::search(const QString &keyword, int limit, int offset, const SearchCallback &callback)
{
    Q_UNUSED(keyword); Q_UNUSED(limit); Q_UNUSED(offset);
    callback(Result<QList<Song>>::failure(Error{ErrorCategory::UpstreamChange, 501, "Not implemented"}));
    return {};
}

QSharedPointer<RequestToken> QQProvider::songDetail(const QString &songId, const SongDetailCallback &callback)
{
    Q_UNUSED(songId);
    callback(Result<Song>::failure(Error{ErrorCategory::UpstreamChange, 501, "Not implemented"}));
    return {};
}

QSharedPointer<RequestToken> QQProvider::playUrl(const QString &songId, const PlayUrlCallback &callback)
{
    Q_UNUSED(songId);
    callback(Result<PlayUrl>::failure(Error{ErrorCategory::UpstreamChange, 501, "Not implemented"}));
    return {};
}

QSharedPointer<RequestToken> QQProvider::lyric(const QString &songId, const LyricCallback &callback)
{
    Q_UNUSED(songId);
    callback(Result<Lyric>::failure(Error{ErrorCategory::UpstreamChange, 501, "Not implemented"}));
    return {};
}

QSharedPointer<RequestToken> QQProvider::cover(const QUrl &coverUrl, const CoverCallback &callback)
{
    Q_UNUSED(coverUrl);
    callback(Result<QByteArray>::failure(Error{ErrorCategory::UpstreamChange, 501, "Not implemented"}));
    return {};
}

QSharedPointer<RequestToken> QQProvider::playlistDetail(const QString &playlistId, const PlaylistDetailCallback &callback)
{
    Q_UNUSED(playlistId);
    callback(Result<PlaylistMeta>::failure(Error{ErrorCategory::UpstreamChange, 501, "Not implemented"}));
    return {};
}

QSharedPointer<RequestToken> QQProvider::playlistTracks(const QString &playlistId, int limit, int offset, const PlaylistTracksCallback &callback)
{
    Q_UNUSED(playlistId); Q_UNUSED(limit); Q_UNUSED(offset);
    callback(Result<PlaylistTracksPage>::failure(Error{ErrorCategory::UpstreamChange, 501, "Not implemented"}));
    return {};
}

QUrl QQProvider::buildUrl(const QString &path, const QList<QPair<QString, QString>> &query) const
{
    QUrl url = apiBase;
    url.setPath(path);
    QUrlQuery q;
    for (const auto &p : query)
    {
        q.addQueryItem(p.first, p.second);
    }
    url.setQuery(q);
    return url;
}

Result<QList<PlaylistMeta>> QQProvider::parseUserPlaylist(const QByteArray &body, bool isCreated) const
{
    // Implementation moved to helper
    return parseUserPlaylistHelper(body);
}

void QQProvider::setCookie(const QString &cookie)
{
    m_cookie = cookie;
}

QSharedPointer<RequestToken> QQProvider::userPlaylist(const QString &uid, const UserPlaylistCallback &callback)
{
    auto fetchCollected = [this, uid, callback](const QList<PlaylistMeta> &createdList) {
        QUrl urlCollect = buildUrl(QStringLiteral("/user/collect/songlist"), {
            {QStringLiteral("id"), uid},
            {QStringLiteral("pageNo"), QStringLiteral("1")},
            {QStringLiteral("pageSize"), QStringLiteral("50")},
            {QStringLiteral("_t"), QString::number(QDateTime::currentMSecsSinceEpoch())}
        });
        HttpRequestOptions opts;
        opts.url = urlCollect;
        opts.timeoutMs = 10000;
        if (!m_cookie.isEmpty())
        {
            opts.headers["Cookie"] = m_cookie.toUtf8();
        }

        client->sendWithRetry(opts, 2, 500, [callback, createdList](Result<HttpResponse> res) {
             QList<PlaylistMeta> finalResults = createdList;
             if (res.ok)
             {
                 auto list = parseUserPlaylistHelper(res.value.body);
                 if (list.ok) finalResults.append(list.value);
             }
             callback(Result<QList<PlaylistMeta>>::success(finalResults));
        });
    };

    if (!m_cookie.isEmpty())
    {
        QUrl url = buildUrl(QStringLiteral("/user/detail"), {
            {QStringLiteral("id"), uid},
            {QStringLiteral("_t"), QString::number(QDateTime::currentMSecsSinceEpoch())}
        });
        HttpRequestOptions opts;
        opts.url = url;
        opts.timeoutMs = 10000;
        opts.headers["Cookie"] = m_cookie.toUtf8();

        return client->sendWithRetry(opts, 2, 500, [this, uid, fetchCollected](Result<HttpResponse> res) {
            if (res.ok) {
                auto parsed = parseUserDetail(res.value.body);
                if (parsed.ok) {
                    fetchCollected(parsed.value);
                    return;
                }
            }
            
            // Fallback to songlist
            QUrl urlSonglist = buildUrl(QStringLiteral("/user/songlist"), {
                {QStringLiteral("id"), uid},
                {QStringLiteral("_t"), QString::number(QDateTime::currentMSecsSinceEpoch())}
            });
            HttpRequestOptions optsSonglist;
            optsSonglist.url = urlSonglist;
            optsSonglist.timeoutMs = 10000;
            optsSonglist.headers["Cookie"] = m_cookie.toUtf8();
            
            client->sendWithRetry(optsSonglist, 2, 500, [fetchCollected](Result<HttpResponse> res2) {
                QList<PlaylistMeta> list;
                if (res2.ok) {
                    auto parsed = parseUserPlaylistHelper(res2.value.body);
                    if (parsed.ok) list = parsed.value;
                }
                fetchCollected(list);
            });
        });
    }
    else
    {
        QUrl url = buildUrl(QStringLiteral("/user/songlist"), {
            {QStringLiteral("id"), uid},
            {QStringLiteral("_t"), QString::number(QDateTime::currentMSecsSinceEpoch())}
        });
        HttpRequestOptions opts;
        opts.url = url;
        opts.timeoutMs = 10000;
        
        return client->sendWithRetry(opts, 2, 500, [fetchCollected](Result<HttpResponse> res) {
            QList<PlaylistMeta> list;
            if (res.ok) {
                auto parsed = parseUserPlaylistHelper(res.value.body);
                if (parsed.ok) list = parsed.value;
            }
            fetchCollected(list);
        });
    }
}

}
