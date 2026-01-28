#include "qq_provider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

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
    QUrl urlCreated = buildUrl(QStringLiteral("/user/songlist"), {{QStringLiteral("id"), uid}});
    HttpRequestOptions opts;
    opts.url = urlCreated;
    opts.timeoutMs = 10000;
    if (!m_cookie.isEmpty())
    {
        opts.headers["Cookie"] = m_cookie.toUtf8();
    }

    // We capture 'this' but only use it to access 'client' and 'buildUrl'.
    // We must ensure 'this' is valid. Usually provider is owned by controller which outlives requests.
    
    return client->sendWithRetry(opts, 2, 500, [this, uid, callback](Result<HttpResponse> res1) {
        QList<PlaylistMeta> allPlaylists;
        if (res1.ok)
        {
            auto list = parseUserPlaylistHelper(res1.value.body);
            if (list.ok) allPlaylists.append(list.value);
        }

        // Now fetch collected
        QUrl urlCollect = buildUrl(QStringLiteral("/user/collect/songlist"), {
            {QStringLiteral("id"), uid},
            {QStringLiteral("pageNo"), QStringLiteral("1")},
            {QStringLiteral("pageSize"), QStringLiteral("50")}
        });
        HttpRequestOptions opts2;
        opts2.url = urlCollect;
        opts2.timeoutMs = 10000;
        if (!m_cookie.isEmpty())
        {
            opts2.headers["Cookie"] = m_cookie.toUtf8();
        }

        client->sendWithRetry(opts2, 2, 500, [callback, allPlaylists](Result<HttpResponse> res2) {
             QList<PlaylistMeta> finalResults = allPlaylists;
             if (res2.ok)
             {
                 auto list = parseUserPlaylistHelper(res2.value.body);
                 if (list.ok) finalResults.append(list.value);
             }
             
             // Even if both fail, we return empty list or partial results?
             // If both failed (empty list), maybe return error?
             // But for now success with empty list is safer.
             callback(Result<QList<PlaylistMeta>>::success(finalResults));
        });
    });
}

}
