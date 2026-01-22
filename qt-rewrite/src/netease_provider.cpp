// NeteaseProvider 实现：通过 netease-cloud-music-api 提供搜索、详情、播放地址与歌词
#include "netease_provider.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
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

bool NeteaseProvider::supportsPlaylistDetail() const
{
	return true;
}

bool NeteaseProvider::supportsPlaylistTracks() const
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
	QSharedPointer<RequestToken> token = QSharedPointer<RequestToken>::create();

	auto cancelIfOuterCancelled = [token](const QSharedPointer<RequestToken> &inner) {
		QObject::connect(token.data(), &RequestToken::cancelled, inner.data(), [inner]() {
			inner->cancel();
		});
	};

	auto readQualityLevel = []() -> QString {
		QSettings settings;
		settings.beginGroup(QStringLiteral("set"));
		QString level = settings.value(QStringLiteral("musicQuality"), QStringLiteral("higher")).toString().trimmed();
		settings.endGroup();
		if (level.isEmpty())
			level = QStringLiteral("higher");
		return level;
	};

	auto readEnabledPlatforms = []() -> QStringList {
		const QStringList allowed = {QStringLiteral("migu"), QStringLiteral("kugou"), QStringLiteral("kuwo"), QStringLiteral("pyncmd"), QStringLiteral("bilibili")};
		QSettings settings;
		settings.beginGroup(QStringLiteral("set"));
		QVariant v = settings.value(QStringLiteral("enabledMusicSources"));
		settings.endGroup();
		QStringList list;
		if (v.isValid())
			list = v.toStringList();
		QStringList filtered;
		for (const QString &s : list)
		{
			QString t = s.trimmed().toLower();
			if (allowed.contains(t) && !filtered.contains(t))
				filtered.append(t);
		}
		if (filtered.isEmpty())
			filtered = allowed;
		return filtered;
	};

	auto isUnblockEnabled = []() -> bool {
		QSettings settings;
		settings.beginGroup(QStringLiteral("set"));
		bool enabled = settings.value(QStringLiteral("enableMusicUnblock"), true).toBool();
		settings.endGroup();
		return enabled;
	};

	auto findMusicApiDir = []() -> QString {
		QSettings settings;
		settings.beginGroup(QStringLiteral("set"));
		QString overrideDir = settings.value(QStringLiteral("musicApiDir"), QString()).toString().trimmed();
		settings.endGroup();
		if (!overrideDir.isEmpty())
		{
			QString dir = QDir::fromNativeSeparators(overrideDir);
			if (QFileInfo::exists(QDir(dir).filePath(QStringLiteral("package.json"))))
				return dir;
		}

		QByteArray envDir = qgetenv("QTREWRITE_MUSIC_API_DIR");
		if (!envDir.isEmpty())
		{
			QString dir = QDir::fromNativeSeparators(QString::fromLocal8Bit(envDir));
			if (QFileInfo::exists(QDir(dir).filePath(QStringLiteral("package.json"))))
				return dir;
		}

		const QStringList roots = {QDir::currentPath(), QCoreApplication::applicationDirPath()};
		for (const QString &root : roots)
		{
			QDir dir(root);
			for (int depth = 0; depth < 10; ++depth)
			{
				QString candidate = dir.filePath(QStringLiteral("qt-rewrite/music-api"));
				if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("package.json"))))
					return candidate;
				candidate = dir.filePath(QStringLiteral("music-api"));
				if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("package.json"))))
					return candidate;
				if (!dir.cdUp())
					break;
			}
		}
		return QString();
	};

	auto startUnblockProcess = [this, songId, callback, token, readEnabledPlatforms, findMusicApiDir](const Song &song) {
		if (token->isCancelled())
		{
			Error e;
			e.category = ErrorCategory::Network;
			e.code = -2;
			e.message = QStringLiteral("Request cancelled");
			callback(Result<PlayUrl>::failure(e));
			return;
		}

		QString apiDir = findMusicApiDir();
		if (apiDir.isEmpty())
		{
			Error e;
			e.category = ErrorCategory::UpstreamChange;
			e.code = -1;
			e.message = QStringLiteral("Embedded music API directory not found");
			callback(Result<PlayUrl>::failure(e));
			return;
		}

		QJsonObject songData;
		songData.insert(QStringLiteral("name"), song.name);
		QJsonArray artists;
		for (const Artist &a : song.artists)
		{
			QJsonObject ao;
			ao.insert(QStringLiteral("name"), a.name);
			artists.append(ao);
		}
		songData.insert(QStringLiteral("artists"), artists);
		QJsonObject album;
		album.insert(QStringLiteral("name"), song.album.name);
		songData.insert(QStringLiteral("album"), album);

		QJsonArray platforms;
		for (const QString &p : readEnabledPlatforms())
			platforms.append(p);

		QString platformsJson = QString::fromUtf8(QJsonDocument(platforms).toJson(QJsonDocument::Compact));
		QString songJson = QString::fromUtf8(QJsonDocument(songData).toJson(QJsonDocument::Compact));

		QProcess *proc = new QProcess(token.data());
		proc->setWorkingDirectory(apiDir);
		proc->setProcessChannelMode(QProcess::SeparateChannels);

		QString js = QStringLiteral(
			"const mod=require('@unblockneteasemusic/server');"
			"const match=mod.default||mod;"
			"(async()=>{"
			"const id=parseInt(process.argv[2],10);"
			"const platforms=JSON.parse(process.argv[3]||'[]');"
			"const song=JSON.parse(process.argv[4]||'{}');"
			"const data=await match(id, platforms, song);"
			"process.stdout.write(JSON.stringify(data||{}));"
			"})().catch(e=>{console.error(e&&e.stack||e);process.exit(1);});"
		);

		proc->setProgram(QStringLiteral("node"));
		proc->setArguments({QStringLiteral("-e"), js, songId, platformsJson, songJson});

		QTimer *timer = new QTimer(proc);
		timer->setSingleShot(true);
		QObject::connect(timer, &QTimer::timeout, proc, [proc]() {
			if (proc->state() != QProcess::NotRunning)
			{
				proc->kill();
			}
		});
		timer->start(12000);

		QObject::connect(token.data(), &RequestToken::cancelled, proc, [proc]() {
			if (proc->state() != QProcess::NotRunning)
				proc->kill();
		});

		QObject::connect(proc, &QProcess::finished, proc, [proc, timer, callback](int exitCode, QProcess::ExitStatus exitStatus) {
			timer->stop();
			QByteArray out = proc->readAllStandardOutput();
			QByteArray err = proc->readAllStandardError();
			if (exitStatus != QProcess::NormalExit || exitCode != 0)
			{
				Error e;
				e.category = ErrorCategory::Unknown;
				e.code = exitCode;
				e.message = QStringLiteral("Unblock music failed");
				e.detail = QString::fromLocal8Bit(err).right(800);
				callback(Result<PlayUrl>::failure(e));
				proc->deleteLater();
				return;
			}

			QJsonParseError pe{};
			QJsonDocument doc = QJsonDocument::fromJson(out, &pe);
			if (pe.error != QJsonParseError::NoError || !doc.isObject())
			{
				Error e;
				e.category = ErrorCategory::Parser;
				e.code = -1;
				e.message = QStringLiteral("Parse unblock result failed");
				e.detail = QString::fromLocal8Bit(out).right(800);
				callback(Result<PlayUrl>::failure(e));
				proc->deleteLater();
				return;
			}
			QJsonObject o = doc.object();
			QString urlStr = o.value(QStringLiteral("url")).toString();
			if (urlStr.trimmed().isEmpty())
			{
				Error e;
				e.category = ErrorCategory::UpstreamChange;
				e.code = 404;
				e.message = QStringLiteral("Play url not found");
				callback(Result<PlayUrl>::failure(e));
				proc->deleteLater();
				return;
			}
			PlayUrl p;
			p.url = QUrl(urlStr);
			p.bitrate = o.value(QStringLiteral("br")).toInt();
			p.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
			callback(Result<PlayUrl>::success(p));
			proc->deleteLater();
		});

		proc->start();
	};

	auto tryGdMusicOrUnblock = [this, callback, token, cancelIfOuterCancelled, startUnblockProcess](const Song &song) {
		if (token->isCancelled())
			return;

		QStringList parts;
		if (!song.name.trimmed().isEmpty())
			parts.append(song.name.trimmed());
		for (const Artist &a : song.artists)
		{
			QString n = a.name.trimmed();
			if (!n.isEmpty())
				parts.append(n);
		}
		QString searchQuery = parts.join(' ').trimmed();
		if (searchQuery.size() < 2)
		{
			startUnblockProcess(song);
			return;
		}

		const QUrl base(QStringLiteral("https://music-api.gdstudio.xyz/api.php"));
		const QStringList sources = {QStringLiteral("joox"), QStringLiteral("tidal"), QStringLiteral("netease")};

		struct State
		{
			int index = 0;
		};
		QSharedPointer<State> state = QSharedPointer<State>::create();
		QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
		*nextFn = [this, base, sources, searchQuery, callback, token, cancelIfOuterCancelled, startUnblockProcess, song, state, nextFn]() {
			if (token->isCancelled())
				return;
			if (state->index >= sources.size())
			{
				startUnblockProcess(song);
				return;
			}
			QString source = sources.at(state->index);
			state->index++;

			QUrl searchUrl = base;
			QUrlQuery q;
			q.addQueryItem(QStringLiteral("types"), QStringLiteral("search"));
			q.addQueryItem(QStringLiteral("source"), source);
			q.addQueryItem(QStringLiteral("name"), searchQuery);
			q.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
			q.addQueryItem(QStringLiteral("pages"), QStringLiteral("1"));
			searchUrl.setQuery(q);

			HttpRequestOptions searchOpts;
			searchOpts.url = searchUrl;
			searchOpts.timeoutMs = 5000;
			QSharedPointer<RequestToken> searchToken = client->sendWithRetry(searchOpts, 1, 300, [this, base, source, callback, token, cancelIfOuterCancelled, startUnblockProcess, song, nextFn](Result<HttpResponse> searchResult) {
				if (token->isCancelled())
					return;
				if (!searchResult.ok)
				{
					(*nextFn)();
					return;
				}

				QJsonParseError pe{};
				QJsonDocument doc = QJsonDocument::fromJson(searchResult.value.body, &pe);
				if (pe.error != QJsonParseError::NoError || !doc.isArray())
				{
					(*nextFn)();
					return;
				}
				QJsonArray arr = doc.array();
				if (arr.isEmpty() || !arr.first().isObject())
				{
					(*nextFn)();
					return;
				}
				QJsonObject first = arr.first().toObject();
				QString trackId = first.value(QStringLiteral("id")).toVariant().toString().trimmed();
				if (trackId.isEmpty())
				{
					(*nextFn)();
					return;
				}
				QString trackSource = first.value(QStringLiteral("source")).toString().trimmed();
				if (trackSource.isEmpty())
					trackSource = source;

				QUrl urlUrl = base;
				QUrlQuery uq;
				uq.addQueryItem(QStringLiteral("types"), QStringLiteral("url"));
				uq.addQueryItem(QStringLiteral("source"), trackSource);
				uq.addQueryItem(QStringLiteral("id"), trackId);
				uq.addQueryItem(QStringLiteral("br"), QStringLiteral("999"));
				urlUrl.setQuery(uq);

				HttpRequestOptions urlOpts;
				urlOpts.url = urlUrl;
				urlOpts.timeoutMs = 5000;
				QSharedPointer<RequestToken> urlToken = client->sendWithRetry(urlOpts, 1, 300, [callback, token, nextFn](Result<HttpResponse> urlResult) {
					if (token->isCancelled())
						return;
					if (!urlResult.ok)
					{
						(*nextFn)();
						return;
					}
					QJsonParseError pe{};
					QJsonDocument doc = QJsonDocument::fromJson(urlResult.value.body, &pe);
					if (pe.error != QJsonParseError::NoError || !doc.isObject())
					{
						(*nextFn)();
						return;
					}
					QJsonObject o = doc.object();
					QString urlStr = o.value(QStringLiteral("url")).toString();
					urlStr.replace('\\', QString());
					if (urlStr.trimmed().isEmpty())
					{
						(*nextFn)();
						return;
					}
					PlayUrl p;
					p.url = QUrl(urlStr);
					p.bitrate = o.value(QStringLiteral("br")).toVariant().toInt();
					p.size = static_cast<qint64>(o.value(QStringLiteral("size")).toVariant().toLongLong());
					callback(Result<PlayUrl>::success(p));
				});
				cancelIfOuterCancelled(urlToken);
			});
			cancelIfOuterCancelled(searchToken);
		};

		(*nextFn)();
	};

	HttpRequestOptions v1;
	v1.url = buildUrl(QStringLiteral("/song/url/v1"), {{QStringLiteral("id"), songId}, {QStringLiteral("level"), readQualityLevel()}, {QStringLiteral("encodeType"), QStringLiteral("aac")}});
	QSharedPointer<RequestToken> v1Token = client->sendWithRetry(v1, 2, 500, [this, songId, callback, token, cancelIfOuterCancelled, isUnblockEnabled, tryGdMusicOrUnblock](Result<HttpResponse> result) {
		if (token->isCancelled())
			return;
		if (result.ok)
		{
			Result<PlayUrl> parsed = parsePlayUrl(result.value.body);
			if (parsed.ok)
			{
				callback(parsed);
				return;
			}
			if (parsed.error.category != ErrorCategory::UpstreamChange)
			{
				callback(Result<PlayUrl>::failure(parsed.error));
				return;
			}
		}
		else
		{
			callback(Result<PlayUrl>::failure(result.error));
			return;
		}

		HttpRequestOptions legacy;
		legacy.url = buildUrl(QStringLiteral("/song/url"), {{QStringLiteral("id"), songId}, {QStringLiteral("br"), QStringLiteral("320000")}});
		QSharedPointer<RequestToken> legacyToken = client->sendWithRetry(legacy, 1, 500, [this, songId, callback, token, isUnblockEnabled, tryGdMusicOrUnblock](Result<HttpResponse> legacyResult) {
			if (token->isCancelled())
				return;
			if (legacyResult.ok)
			{
				Result<PlayUrl> parsed = parsePlayUrl(legacyResult.value.body);
				if (parsed.ok)
				{
					callback(parsed);
					return;
				}
				if (parsed.error.category != ErrorCategory::UpstreamChange)
				{
					callback(Result<PlayUrl>::failure(parsed.error));
					return;
				}
			}
			else
			{
				callback(Result<PlayUrl>::failure(legacyResult.error));
				return;
			}

			if (!isUnblockEnabled())
			{
				Error e;
				e.category = ErrorCategory::UpstreamChange;
				e.code = 404;
				e.message = QStringLiteral("Play url not found");
				callback(Result<PlayUrl>::failure(e));
				return;
			}

			HttpRequestOptions detail;
			detail.url = buildUrl(QStringLiteral("/song/detail"), {{QStringLiteral("ids"), songId}});
			QSharedPointer<RequestToken> detailToken = client->sendWithRetry(detail, 1, 500, [this, callback, token, tryGdMusicOrUnblock](Result<HttpResponse> detailResult) {
				if (token->isCancelled())
					return;
				if (!detailResult.ok)
				{
					callback(Result<PlayUrl>::failure(detailResult.error));
					return;
				}
				Result<Song> parsedSong = parseSongDetail(detailResult.value.body);
				if (!parsedSong.ok)
				{
					callback(Result<PlayUrl>::failure(parsedSong.error));
					return;
				}
				tryGdMusicOrUnblock(parsedSong.value);
			});
			QObject::connect(token.data(), &RequestToken::cancelled, detailToken.data(), [detailToken]() {
				detailToken->cancel();
			});
		});
		cancelIfOuterCancelled(legacyToken);
		QObject::connect(token.data(), &RequestToken::cancelled, legacyToken.data(), [legacyToken]() {
			legacyToken->cancel();
		});
	});

	cancelIfOuterCancelled(v1Token);
	return token;
}

QSharedPointer<RequestToken> NeteaseProvider::lyric(const QString &songId, const LyricCallback &callback)
{
	QSharedPointer<RequestToken> token = QSharedPointer<RequestToken>::create();

	HttpRequestOptions opts;
	opts.url = buildUrl(QStringLiteral("/lyric/new"), {{QStringLiteral("id"), songId}});
	QSharedPointer<RequestToken> first = client->sendWithRetry(opts, 2, 500, [this, songId, callback, token](Result<HttpResponse> result) {
		if (token->isCancelled())
			return;
		if (!result.ok)
		{
			callback(Result<Lyric>::failure(result.error));
			return;
		}
		Result<Lyric> parsed = parseLyric(result.value.body);
		if (parsed.ok && !parsed.value.lines.isEmpty())
		{
			callback(parsed);
			return;
		}

		HttpRequestOptions legacy;
		legacy.url = buildUrl(QStringLiteral("/lyric"), {{QStringLiteral("id"), songId}});
		QSharedPointer<RequestToken> second = client->sendWithRetry(legacy, 1, 500, [this, callback, token](Result<HttpResponse> legacyResult) {
			if (token->isCancelled())
				return;
			if (!legacyResult.ok)
			{
				callback(Result<Lyric>::failure(legacyResult.error));
				return;
			}
			callback(parseLyric(legacyResult.value.body));
		});
		QObject::connect(token.data(), &RequestToken::cancelled, second.data(), [second]() {
			second->cancel();
		});
	});

	QObject::connect(token.data(), &RequestToken::cancelled, first.data(), [first]() {
		first->cancel();
	});
	return token;
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

QSharedPointer<RequestToken> NeteaseProvider::playlistDetail(const QString &playlistId, const PlaylistDetailCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl(QStringLiteral("/playlist/detail"), {{QStringLiteral("id"), playlistId}});
	return client->sendWithRetry(opts, 2, 500, [this, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<PlaylistMeta>::failure(result.error));
			return;
		}
		callback(parsePlaylistDetail(result.value.body));
	});
}

QSharedPointer<RequestToken> NeteaseProvider::playlistTracks(const QString &playlistId, int limit, int offset, const PlaylistTracksCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl(QStringLiteral("/playlist/track/all"), {{QStringLiteral("id"), playlistId}, {QStringLiteral("limit"), QString::number(limit > 0 ? limit : 50)}, {QStringLiteral("offset"), QString::number(offset > 0 ? offset : 0)}});
	return client->sendWithRetry(opts, 2, 500, [this, playlistId, limit, offset, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<PlaylistTracksPage>::failure(result.error));
			return;
		}
		callback(parsePlaylistTracks(playlistId, limit, offset, result.value.body));
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
		s.providerId = id();
		s.source = id();
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
	s.providerId = id();
	s.source = id();
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
	QString urlStr = o.value(QStringLiteral("url")).toString();
	if (urlStr.trimmed().isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("Play url not found");
		return Result<PlayUrl>::failure(e);
	}
	PlayUrl p;
	p.url = QUrl(urlStr);
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
	QString rawTLrc = root.value(QStringLiteral("tlyric")).toObject().value(QStringLiteral("lyric")).toString();
	QString rawYrc = root.value(QStringLiteral("yrc")).toObject().value(QStringLiteral("lyric")).toString();
	if (rawLrc.trimmed().isEmpty() && !rawYrc.trimmed().isEmpty())
	{
		QJsonParseError ype{};
		QJsonDocument ydoc = QJsonDocument::fromJson(rawYrc.toUtf8(), &ype);
		if (ype.error == QJsonParseError::NoError && ydoc.isArray())
		{
			Lyric lyric;
			QJsonArray arr = ydoc.array();
			lyric.lines.reserve(arr.size());
			for (const QJsonValue &v : arr)
			{
				if (!v.isObject())
					continue;
				QJsonObject o = v.toObject();
				qint64 t = static_cast<qint64>(o.value(QStringLiteral("t")).toVariant().toLongLong());
				QJsonArray chunks = o.value(QStringLiteral("c")).toArray();
				QString text;
				text.reserve(64);
				for (const QJsonValue &cv : chunks)
				{
					if (!cv.isObject())
						continue;
					QString tx = cv.toObject().value(QStringLiteral("tx")).toString();
					if (!tx.isEmpty())
						text.append(tx);
				}
				text = text.trimmed();
				if (text.isEmpty())
					continue;
				LyricLine ll;
				ll.timeMs = t;
				ll.text = text;
				lyric.lines.append(ll);
			}
			if (!lyric.lines.isEmpty())
			{
				std::sort(lyric.lines.begin(), lyric.lines.end(), [](const LyricLine &a, const LyricLine &b) {
					if (a.timeMs != b.timeMs)
						return a.timeMs < b.timeMs;
					return a.text < b.text;
				});
				return Result<Lyric>::success(lyric);
			}
		}
	}
	if (!rawLrc.isEmpty() && !rawTLrc.isEmpty())
	{
		QMap<QString, QString> original;
		QMap<QString, QString> translated;
		auto collect = [](const QString &text, QMap<QString, QString> &out) {
			QString normalized = text;
			normalized.replace("\r\n", "\n");
			normalized.replace("\r", "\n");
			QStringList lines = normalized.split('\n');
			QRegularExpression re(QStringLiteral("\\[(\\d{1,2}:\\d{2}(?:\\.\\d{1,3})?)\\]"));
			for (const QString &line : lines)
			{
				QRegularExpressionMatchIterator it = re.globalMatch(line);
				QStringList tags;
				int lastEnd = -1;
				while (it.hasNext())
				{
					QRegularExpressionMatch m = it.next();
					tags.append(QStringLiteral("[") + m.captured(1) + QStringLiteral("]"));
					lastEnd = m.capturedEnd();
				}
				if (tags.isEmpty())
					continue;
				QString content = (lastEnd >= 0) ? line.mid(lastEnd).trimmed() : line.trimmed();
				if (content.isEmpty())
					continue;
				for (const QString &tag : tags)
					out.insert(tag, content);
			}
		};
		collect(rawLrc, original);
		collect(rawTLrc, translated);
		QStringList merged;
		merged.reserve(original.size() + translated.size());
		for (auto it = original.cbegin(); it != original.cend(); ++it)
		{
			merged.append(it.key() + it.value());
			auto tr = translated.constFind(it.key());
			if (tr != translated.cend())
				merged.append(it.key() + tr.value());
		}
		std::sort(merged.begin(), merged.end());
		rawLrc = merged.join('\n');
	}
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

Result<PlaylistMeta> NeteaseProvider::parsePlaylistDetail(const QByteArray &body) const
{
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse playlist detail response failed");
		return Result<PlaylistMeta>::failure(e);
	}
	QJsonObject root = doc.object();
	QJsonObject playlistObj = root.value(QStringLiteral("playlist")).toObject();
	if (playlistObj.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("Playlist not found");
		return Result<PlaylistMeta>::failure(e);
	}
	PlaylistMeta meta;
	meta.id = playlistObj.value(QStringLiteral("id")).toVariant().toString();
	meta.name = playlistObj.value(QStringLiteral("name")).toString();
	meta.coverUrl = QUrl(playlistObj.value(QStringLiteral("coverImgUrl")).toString());
	meta.description = playlistObj.value(QStringLiteral("description")).toString();
	meta.trackCount = playlistObj.value(QStringLiteral("trackCount")).toInt();
	return Result<PlaylistMeta>::success(meta);
}

Result<PlaylistTracksPage> NeteaseProvider::parsePlaylistTracks(const QString &playlistId, int limit, int offset, const QByteArray &body) const
{
	QJsonParseError err{};
	QJsonDocument doc = QJsonDocument::fromJson(body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
	{
		Error e;
		e.category = ErrorCategory::Parser;
		e.code = -1;
		e.message = QStringLiteral("Parse playlist tracks response failed");
		return Result<PlaylistTracksPage>::failure(e);
	}
	QJsonObject root = doc.object();
	QJsonArray songsArr = root.value(QStringLiteral("songs")).toArray();
	QList<Song> songs;
	songs.reserve(songsArr.size());
	for (const QJsonValue &v : songsArr)
	{
		QJsonObject o = v.toObject();
		Song s;
		s.providerId = id();
		s.source = id();
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
	PlaylistTracksPage page;
	page.playlistId = playlistId;
	page.songs = songs;
	page.limit = limit > 0 ? limit : songs.size();
	page.offset = offset > 0 ? offset : 0;
	page.total = root.value(QStringLiteral("total")).toInt();
	if (page.total <= 0)
		page.total = page.offset + page.songs.size();
	return Result<PlaylistTracksPage>::success(page);
}

}
