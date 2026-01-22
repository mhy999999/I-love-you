#include "gdstudio_provider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include "json_utils.h"

namespace App
{

namespace
{

Result<QString> readFirstString(const QJsonObject &obj, const QList<QString> &keys, bool required)
{
	for (const QString &k : keys)
	{
		if (!obj.contains(k) || obj.value(k).isNull())
			continue;
		Result<QString> r = Json::readString(obj, k, true);
		if (r.ok)
			return r;
	}
	if (required)
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = 1;
		e.message = QStringLiteral("Missing field");
		return Result<QString>::failure(e);
	}
	return Result<QString>::success(QString());
}

QString normalizeArtistsString(const QString &s)
{
	QString t = s;
	t.replace(QStringLiteral("\\"), QString());
	return t.trimmed();
}

}

GdStudioProvider::GdStudioProvider(HttpClient *httpClient, QObject *parent)
	: IProvider(parent)
	, client(httpClient)
	, apiBase(QStringLiteral("https://music-api.gdstudio.xyz/api.php"))
{
}

QString GdStudioProvider::id() const
{
	return QStringLiteral("gdstudio");
}

QString GdStudioProvider::displayName() const
{
	return QStringLiteral("GD Studio");
}

bool GdStudioProvider::supportsSongDetail() const
{
	return false;
}

bool GdStudioProvider::supportsLyric() const
{
	return false;
}

bool GdStudioProvider::supportsCover() const
{
	return false;
}

bool GdStudioProvider::supportsPlaylistDetail() const
{
	return false;
}

bool GdStudioProvider::supportsPlaylistTracks() const
{
	return false;
}

QSharedPointer<RequestToken> GdStudioProvider::search(const QString &keyword, int limit, const SearchCallback &callback)
{
	HttpRequestOptions opts;
	QUrl url = apiBase;
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("types"), QStringLiteral("search"));
	q.addQueryItem(QStringLiteral("source"), QStringLiteral("netease"));
	q.addQueryItem(QStringLiteral("name"), keyword);
	q.addQueryItem(QStringLiteral("count"), QStringLiteral("%1").arg(limit > 0 ? limit : 30));
	q.addQueryItem(QStringLiteral("pages"), QStringLiteral("1"));
	url.setQuery(q);
	opts.url = url;
	opts.timeoutMs = 6000;

	return client->sendWithRetry(opts, 1, 300, [this, limit, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<QList<Song>>::failure(result.error));
			return;
		}
		callback(parseSearch(result.value.body, limit));
	});
}

QSharedPointer<RequestToken> GdStudioProvider::songDetail(const QString &songId, const SongDetailCallback &callback)
{
	Q_UNUSED(songId);
	Error e;
	e.category = ErrorCategory::UpstreamChange;
	e.code = 501;
	e.message = QStringLiteral("Song detail not supported");
	callback(Result<Song>::failure(e));
	return {};
}

QSharedPointer<RequestToken> GdStudioProvider::playUrl(const QString &songId, const PlayUrlCallback &callback)
{
	QString source = QStringLiteral("netease");
	QString idPart = songId;
	int sep = songId.indexOf(QLatin1Char(':'));
	if (sep > 0)
	{
		source = songId.left(sep).trimmed();
		idPart = songId.mid(sep + 1).trimmed();
	}
	else
	{
		idPart = songId.trimmed();
	}
	if (source.compare(QStringLiteral("qq"), Qt::CaseInsensitive) == 0)
		source = QStringLiteral("tencent");
	if (idPart.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = 1;
		e.message = QStringLiteral("Invalid song id");
		callback(Result<PlayUrl>::failure(e));
		return {};
	}

	HttpRequestOptions opts;
	QUrl url = apiBase;
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("types"), QStringLiteral("url"));
	q.addQueryItem(QStringLiteral("source"), source);
	q.addQueryItem(QStringLiteral("id"), idPart);
	q.addQueryItem(QStringLiteral("br"), QStringLiteral("999"));
	url.setQuery(q);
	opts.url = url;
	opts.timeoutMs = 6000;

	return client->sendWithRetry(opts, 1, 300, [this, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<PlayUrl>::failure(result.error));
			return;
		}
		callback(parsePlayUrl(result.value.body));
	});
}

QSharedPointer<RequestToken> GdStudioProvider::lyric(const QString &songId, const LyricCallback &callback)
{
	Q_UNUSED(songId);
	Error e;
	e.category = ErrorCategory::UpstreamChange;
	e.code = 501;
	e.message = QStringLiteral("Lyric not supported");
	callback(Result<Lyric>::failure(e));
	return {};
}

QSharedPointer<RequestToken> GdStudioProvider::cover(const QUrl &coverUrl, const CoverCallback &callback)
{
	Q_UNUSED(coverUrl);
	Error e;
	e.category = ErrorCategory::UpstreamChange;
	e.code = 501;
	e.message = QStringLiteral("Cover not supported");
	callback(Result<QByteArray>::failure(e));
	return {};
}

QSharedPointer<RequestToken> GdStudioProvider::playlistDetail(const QString &playlistId, const PlaylistDetailCallback &callback)
{
	Q_UNUSED(playlistId);
	Error e;
	e.category = ErrorCategory::UpstreamChange;
	e.code = 501;
	e.message = QStringLiteral("Playlist not supported");
	callback(Result<PlaylistMeta>::failure(e));
	return {};
}

QSharedPointer<RequestToken> GdStudioProvider::playlistTracks(const QString &playlistId, int limit, int offset, const PlaylistTracksCallback &callback)
{
	Q_UNUSED(playlistId);
	Q_UNUSED(limit);
	Q_UNUSED(offset);
	Error e;
	e.category = ErrorCategory::UpstreamChange;
	e.code = 501;
	e.message = QStringLiteral("Playlist tracks not supported");
	callback(Result<PlaylistTracksPage>::failure(e));
	return {};
}

Result<QList<Song>> GdStudioProvider::parseSearch(const QByteArray &body, int limit) const
{
	QJsonParseError pe{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
	if (pe.error != QJsonParseError::NoError || !doc.isArray())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse search response failed");
		e.detail = QString::fromUtf8(body).right(800);
		return Result<QList<Song>>::failure(e);
	}

	QJsonArray arr = doc.array();
	QList<Song> songs;
	int maxCount = limit > 0 ? limit : 30;
	for (const QJsonValue &v : arr)
	{
		if (songs.size() >= maxCount)
			break;
		if (!v.isObject())
			continue;
		QJsonObject o = v.toObject();

		Result<QString> rawId = readFirstString(o, {QStringLiteral("id")}, true);
		if (!rawId.ok)
			continue;

		Result<QString> src = readFirstString(o, {QStringLiteral("source")}, false);
		QString source = src.ok ? src.value.trimmed() : QString();
		if (source.isEmpty())
			source = QStringLiteral("netease");

		Result<QString> name = readFirstString(o, {QStringLiteral("name"), QStringLiteral("title"), QStringLiteral("song")}, false);
		QString title = name.ok ? name.value.trimmed() : QString();
		if (title.isEmpty())
			title = rawId.value;

		QString artistText;
		Result<QString> artist = readFirstString(o, {QStringLiteral("artist"), QStringLiteral("artists")}, false);
		if (artist.ok)
			artistText = normalizeArtistsString(artist.value);

		Song s;
		s.providerId = id();
		s.source = source;
		s.id = rawId.value.trimmed();
		s.name = title;
		if (!artistText.isEmpty())
		{
			Artist a;
			a.name = artistText;
			s.artists.append(a);
		}

		Result<QString> album = readFirstString(o, {QStringLiteral("album")}, false);
		if (album.ok)
			s.album.name = album.value.trimmed();

		Result<QString> cover = readFirstString(o, {QStringLiteral("pic"), QStringLiteral("cover"), QStringLiteral("image")}, false);
		if (cover.ok)
		{
			QUrl coverUrl(cover.value.trimmed());
			if (coverUrl.isValid())
				s.album.coverUrl = coverUrl;
		}

		Result<qint64> dur = Json::readInt64(o, QStringLiteral("duration"), false);
		if (!dur.ok)
			dur = Json::readInt64(o, QStringLiteral("time"), false);
		if (dur.ok && dur.value > 0)
		{
			if (dur.value < 1000)
				s.durationMs = dur.value * 1000;
			else
				s.durationMs = dur.value;
		}

		songs.append(s);
	}
	return Result<QList<Song>>::success(songs);
}

Result<PlayUrl> GdStudioProvider::parsePlayUrl(const QByteArray &body) const
{
	QJsonParseError pe{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
	if (pe.error != QJsonParseError::NoError || !doc.isObject())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse play url response failed");
		e.detail = QString::fromUtf8(body).right(800);
		return Result<PlayUrl>::failure(e);
	}
	QJsonObject o = doc.object();
	Result<QString> url = Json::readString(o, QStringLiteral("url"), true);
	if (!url.ok)
		return Result<PlayUrl>::failure(url.error);
	QString urlStr = url.value;
	urlStr.replace('\\', QString());
	if (urlStr.trimmed().isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("Play url not found");
		return Result<PlayUrl>::failure(e);
	}

	PlayUrl p;
	p.url = QUrl(urlStr.trimmed());
	Result<int> br = Json::readInt(o, QStringLiteral("br"), false);
	if (br.ok)
		p.bitrate = br.value;
	Result<qint64> size = Json::readInt64(o, QStringLiteral("size"), false);
	if (size.ok)
		p.size = size.value;
	return Result<PlayUrl>::success(p);
}

}
