// NeteaseProvider 实现：通过 netease-cloud-music-api 提供搜索、详情、播放地址与歌词
#include "netease_provider.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

#include "json_utils.h"

namespace App
{

NeteaseProvider::NeteaseProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent)
	: IProvider(parent)
	, client(httpClient)
	, apiBase(baseUrl)
{
}

QString NeteaseProvider::id() const
{
	return QStringLiteral("netease");
}

QString NeteaseProvider::displayName() const
{
	return QStringLiteral("网易云音乐");
}

bool NeteaseProvider::supportsLyric() const
{
	return true;
}

bool NeteaseProvider::supportsCover() const
{
	return true;
}

QUrl NeteaseProvider::buildUrl(const QString &path, const QList<QPair<QString, QString>> &query) const
{
	QUrl url = apiBase.resolved(QUrl(path));
	QUrlQuery q;
	for (const auto &pair : query)
		q.addQueryItem(pair.first, pair.second);
	url.setQuery(q);
	return url;
}

QSharedPointer<RequestToken> NeteaseProvider::search(const QString &keyword, int limit, const SearchCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl(QStringLiteral("/cloudsearch"), {{QStringLiteral("keywords"), keyword}, {QStringLiteral("type"), QStringLiteral("1")}, {QStringLiteral("limit"), QString::number(limit > 0 ? limit : 30)}});
	return client->sendWithRetry(opts, 2, 500, [this, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<QList<Song>>::failure(result.error));
			return;
		}
		Result<QList<Song>> parsed = parseSearchSongs(result.value.body);
		callback(parsed);
	});
}

QSharedPointer<RequestToken> NeteaseProvider::songDetail(const QString &songId, const SongDetailCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl(QStringLiteral("/song/detail"), {{QStringLiteral("ids"), songId}});
	return client->sendWithRetry(opts, 2, 500, [this, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<Song>::failure(result.error));
			return;
		}
		Result<Song> parsed = parseSongDetail(result.value.body);
		callback(parsed);
	});
}

QSharedPointer<RequestToken> NeteaseProvider::playUrl(const QString &songId, const PlayUrlCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl(QStringLiteral("/song/url"), {{QStringLiteral("id"), songId}, {QStringLiteral("br"), QStringLiteral("320000")}});
	return client->sendWithRetry(opts, 2, 500, [this, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<PlayUrl>::failure(result.error));
			return;
		}
		Result<PlayUrl> parsed = parsePlayUrl(result.value.body);
		callback(parsed);
	});
}

QSharedPointer<RequestToken> NeteaseProvider::lyric(const QString &songId, const LyricCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl(QStringLiteral("/lyric"), {{QStringLiteral("id"), songId}});
	return client->sendWithRetry(opts, 2, 500, [this, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<Lyric>::failure(result.error));
			return;
		}
		Result<Lyric> parsed = parseLyric(result.value.body);
		callback(parsed);
	});
}

QSharedPointer<RequestToken> NeteaseProvider::cover(const QUrl &coverUrl, const CoverCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = coverUrl;
	opts.headers.insert("Accept", "image/avif,image/webp,image/apng,image/*,*/*;q=0.8");
	opts.timeoutMs = 15000;
	return client->sendWithRetry(opts, 2, 500, [callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<QByteArray>::failure(result.error));
			return;
		}
		callback(Result<QByteArray>::success(result.value.body));
	});
}

Result<QList<Song>> NeteaseProvider::parseSearchSongs(const QByteArray &body) const
{
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse search response failed");
		return Result<QList<Song>>::failure(e);
	}
	QJsonObject root = doc.object();
	QJsonObject resultObj = root.value(QStringLiteral("result")).toObject();
	QJsonArray songsArr = resultObj.value(QStringLiteral("songs")).toArray();
	QList<Song> songs;
	for (const QJsonValue &v : songsArr)
	{
		QJsonObject o = v.toObject();
		Song s;
		s.id = o.value(QStringLiteral("id")).toVariant().toString();
		s.name = o.value(QStringLiteral("name")).toString();
		QJsonArray artistArr = o.value(QStringLiteral("ar")).toArray();
		for (const QJsonValue &av : artistArr)
		{
			QJsonObject ao = av.toObject();
			Artist a;
			a.id = ao.value(QStringLiteral("id")).toVariant().toString();
			a.name = ao.value(QStringLiteral("name")).toString();
			s.artists.append(a);
		}
		QJsonObject al = o.value(QStringLiteral("al")).toObject();
		s.album.id = al.value(QStringLiteral("id")).toVariant().toString();
		s.album.name = al.value(QStringLiteral("name")).toString();
		s.album.coverUrl = QUrl(al.value(QStringLiteral("picUrl")).toString());
		s.durationMs = o.value(QStringLiteral("dt")).toInteger();
		songs.append(s);
	}
	return Result<QList<Song>>::success(songs);
}

Result<Song> NeteaseProvider::parseSongDetail(const QByteArray &body) const
{
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse song detail response failed");
		return Result<Song>::failure(e);
	}
	QJsonObject root = doc.object();
	QJsonArray songsArr = root.value(QStringLiteral("songs")).toArray();
	if (songsArr.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("Song not found");
		return Result<Song>::failure(e);
	}
	QJsonObject o = songsArr.first().toObject();
	Song s;
	s.id = o.value(QStringLiteral("id")).toVariant().toString();
	s.name = o.value(QStringLiteral("name")).toString();
	QJsonObject al = o.value(QStringLiteral("al")).toObject();
	s.album.id = al.value(QStringLiteral("id")).toVariant().toString();
	s.album.name = al.value(QStringLiteral("name")).toString();
	s.album.coverUrl = QUrl(al.value(QStringLiteral("picUrl")).toString());
	QJsonArray artistArr = o.value(QStringLiteral("ar")).toArray();
	for (const QJsonValue &av : artistArr)
	{
		QJsonObject ao = av.toObject();
		Artist a;
		a.id = ao.value(QStringLiteral("id")).toVariant().toString();
		a.name = ao.value(QStringLiteral("name")).toString();
		s.artists.append(a);
	}
	s.durationMs = o.value(QStringLiteral("dt")).toInteger();
	return Result<Song>::success(s);
}

Result<PlayUrl> NeteaseProvider::parsePlayUrl(const QByteArray &body) const
{
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse play url response failed");
		return Result<PlayUrl>::failure(e);
	}
	QJsonObject root = doc.object();
	QJsonArray arr = root.value(QStringLiteral("data")).toArray();
	if (arr.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("Play url not found");
		return Result<PlayUrl>::failure(e);
	}
	QJsonObject o = arr.first().toObject();
	PlayUrl p;
	p.url = QUrl(o.value(QStringLiteral("url")).toString());
	p.bitrate = o.value(QStringLiteral("br")).toInt();
	p.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
	return Result<PlayUrl>::success(p);
}

Result<Lyric> NeteaseProvider::parseLyric(const QByteArray &body) const
{
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse lyric response failed");
		return Result<Lyric>::failure(e);
	}
	QJsonObject root = doc.object();
	QString rawLrc = root.value(QStringLiteral("lrc")).toObject().value(QStringLiteral("lyric")).toString();
	Lyric lyric;
	QString normalized = rawLrc;
	normalized.replace("\r\n", "\n");
	normalized.replace("\r", "\n");
	QStringList lines = normalized.split('\n');
	bool anyTimestamp = false;
	QRegularExpression re(QStringLiteral("\\[(\\d{1,2}):(\\d{2})(?:\\.(\\d{1,3}))?\\]"));
	for (const QString &line : lines)
	{
		if (line.trimmed().isEmpty())
			continue;
		QRegularExpressionMatchIterator it = re.globalMatch(line);
		QList<qint64> times;
		int lastEnd = -1;
		while (it.hasNext())
		{
			QRegularExpressionMatch m = it.next();
			bool okMin = false;
			bool okSec = false;
			int minutes = m.captured(1).toInt(&okMin);
			int seconds = m.captured(2).toInt(&okSec);
			if (!okMin || !okSec)
				continue;
			int millis = 0;
			QString msPart = m.captured(3);
			if (!msPart.isEmpty())
			{
				bool okMs = false;
				int rawMs = msPart.toInt(&okMs);
				if (okMs)
				{
					if (msPart.size() == 1)
						millis = rawMs * 100;
					else if (msPart.size() == 2)
						millis = rawMs * 10;
					else
						millis = rawMs;
				}
			}
			times.append(static_cast<qint64>(minutes) * 60000 + static_cast<qint64>(seconds) * 1000 + millis);
			lastEnd = m.capturedEnd();
		}
		QString text = (lastEnd >= 0) ? line.mid(lastEnd).trimmed() : line.trimmed();
		if (!times.isEmpty())
		{
			anyTimestamp = true;
			for (qint64 t : times)
			{
				LyricLine ll;
				ll.timeMs = t;
				ll.text = text;
				lyric.lines.append(ll);
			}
		}
		else
		{
			LyricLine ll;
			ll.timeMs = 0;
			ll.text = text;
			lyric.lines.append(ll);
		}
	}
	if (anyTimestamp)
	{
		std::sort(lyric.lines.begin(), lyric.lines.end(), [](const LyricLine &a, const LyricLine &b) {
			if (a.timeMs != b.timeMs)
				return a.timeMs < b.timeMs;
			return a.text < b.text;
		});
		QList<LyricLine> deduped;
		deduped.reserve(lyric.lines.size());
		for (const LyricLine &ll : lyric.lines)
		{
			if (!deduped.isEmpty() && deduped.last().timeMs == ll.timeMs && deduped.last().text == ll.text)
				continue;
			deduped.append(ll);
		}
		lyric.lines = deduped;
	}
	return Result<Lyric>::success(lyric);
}

}
