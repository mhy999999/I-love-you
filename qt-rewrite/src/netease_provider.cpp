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
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QUrlQuery>

#include "logger.h"
#include "json_utils.h"

namespace App
{

NeteaseProvider::NeteaseProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent)
	: IProvider(parent)
	, client(httpClient)
	, apiBase(baseUrl)
{
#ifdef QT_DEBUG
	if (qEnvironmentVariableIsSet("APP_SELFTEST_GD_MATCHING"))
	{
		{
			const QUrl base(QStringLiteral("https://music-api.gdstudio.xyz/api.php"));
			QUrl u = base;
			QUrlQuery q;
			q.addQueryItem(QStringLiteral("types"), QStringLiteral("search"));
			q.addQueryItem(QStringLiteral("source"), QStringLiteral("tencent"));
			q.addQueryItem(QStringLiteral("name"), QStringLiteral("刀马旦 李玟 周杰伦"));
			q.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
			q.addQueryItem(QStringLiteral("pages"), QStringLiteral("1"));
			u.setQuery(q);
			Q_ASSERT(!u.toString(QUrl::FullyEncoded).contains(QLatin1Char(' ')));
		}

		const int titleExact = 120;
		const int durationClose = 60;
		const int albumExact = 35;
		const int artistSingle = -60 - 80;
		const int artistFull = 90 + 25;
		Q_ASSERT(titleExact + durationClose + albumExact + artistSingle < 200);
		Q_ASSERT(titleExact + durationClose + albumExact + artistFull >= 200);

		{
			Song song;
			song.name = QStringLiteral("刀马旦");
			Artist a1;
			a1.name = QStringLiteral("李玟");
			Artist a2;
			a2.name = QStringLiteral("周杰伦");
			song.artists = {a1, a2};
			song.album.name = QStringLiteral("Promise");
			song.durationMs = 267000;

			QJsonArray arr;
			arr.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("1")},
							 {QStringLiteral("name"), QStringLiteral("刀马旦")},
							 {QStringLiteral("artist"), QStringLiteral("李玟")},
							 {QStringLiteral("album"), QStringLiteral("Promise")},
							 {QStringLiteral("duration"), 267000}});
			arr.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("2")},
							 {QStringLiteral("name"), QStringLiteral("刀马旦")},
							 {QStringLiteral("artist"), QStringLiteral("李玟 周杰伦")},
							 {QStringLiteral("album"), QStringLiteral("Promise")},
							 {QStringLiteral("duration"), 267000}});

			auto normalizeText = [](QString s) -> QString {
				s = s.trimmed().toLower();
				s.replace('\\', QChar());
				static const QRegularExpression brackets(QStringLiteral("[\\(\\[\\{\\uff08\\u3010].*?[\\)\\]\\}\\uff09\\u3011]"));
				s.replace(brackets, QString());
				static const QRegularExpression nonWord(QStringLiteral("[^\\p{L}\\p{N}]+"));
				s.replace(nonWord, QString());
				return s;
			};
			auto extractFlags = [](const QString &raw) -> QSet<QString> {
				QSet<QString> flags;
				auto addIf = [&flags, &raw](const QString &key, const QStringList &tokens) {
					for (const QString &t : tokens)
					{
						if (raw.contains(t, Qt::CaseInsensitive))
						{
							flags.insert(key);
							return;
						}
					}
				};
				addIf(QStringLiteral("live"), {QStringLiteral("live"), QStringLiteral("现场"), QStringLiteral("演唱会")});
				addIf(QStringLiteral("remix"), {QStringLiteral("remix"), QStringLiteral("混音")});
				addIf(QStringLiteral("dj"), {QStringLiteral("dj")});
				addIf(QStringLiteral("acoustic"), {QStringLiteral("acoustic"), QStringLiteral("不插电")});
				addIf(QStringLiteral("demo"), {QStringLiteral("demo")});
				addIf(QStringLiteral("inst"), {QStringLiteral("inst"), QStringLiteral("instrumental"), QStringLiteral("伴奏"), QStringLiteral("纯音乐")});
				addIf(QStringLiteral("cover"), {QStringLiteral("cover"), QStringLiteral("翻唱")});
				return flags;
			};
			auto maybeDurationMs = [](const QJsonObject &obj) -> qint64 {
				qint64 v = 0;
				if (obj.contains(QStringLiteral("duration")))
					v = static_cast<qint64>(obj.value(QStringLiteral("duration")).toVariant().toLongLong());
				if (v <= 0 && obj.contains(QStringLiteral("time")))
					v = static_cast<qint64>(obj.value(QStringLiteral("time")).toVariant().toLongLong());
				if (v > 0 && v < 1000)
					v = v * 1000;
				return v;
			};

			QString songTitleNorm = normalizeText(song.name);
			QStringList songArtistNorms;
			for (const Artist &a : song.artists)
			{
				QString an = normalizeText(a.name);
				if (!an.isEmpty() && !songArtistNorms.contains(an))
					songArtistNorms.append(an);
			}
			QString albumNorm = normalizeText(song.album.name);
			QSet<QString> songFlags = extractFlags(song.name);

			auto readFirstString = [](const QJsonObject &obj, const QStringList &keys) -> QString {
				for (const QString &k : keys)
				{
					if (!obj.contains(k) || obj.value(k).isNull())
						continue;
					QJsonValue v = obj.value(k);
					if (v.isString())
						return v.toString();
					if (v.isDouble())
						return QString::number(v.toDouble(), 'f', 0);
					if (v.isBool())
						return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
				}
				return QString();
			};

			auto calcScore = [&](const QJsonObject &o) -> int {
				QString candTitle = readFirstString(o, {QStringLiteral("name"), QStringLiteral("title"), QStringLiteral("song")}).trimmed();
				QString candArtist = readFirstString(o, {QStringLiteral("artist"), QStringLiteral("artists"), QStringLiteral("singer")}).trimmed();
				QString candAlbum = readFirstString(o, {QStringLiteral("album"), QStringLiteral("al"), QStringLiteral("albumName")}).trimmed();
				QString candTitleNorm = normalizeText(candTitle);
				QString candArtistNorm = normalizeText(candArtist);
				QString candAlbumNorm = normalizeText(candAlbum);
				int score = 0;
				if (!songTitleNorm.isEmpty() && !candTitleNorm.isEmpty())
				{
					if (candTitleNorm == songTitleNorm)
						score += 120;
					else if (candTitleNorm.contains(songTitleNorm) || songTitleNorm.contains(candTitleNorm))
						score += 90;
					else
						score += 10;
				}

				int artistHit = 0;
				for (const QString &an : songArtistNorms)
				{
					if (!an.isEmpty() && candArtistNorm.contains(an))
						artistHit++;
				}
				if (!songArtistNorms.isEmpty())
				{
					if (artistHit >= 2)
						score += 90;
					else if (artistHit == 1)
						score -= 60;
					else
						score -= 120;
					if (artistHit == songArtistNorms.size())
						score += 25;
					if (songArtistNorms.size() >= 2 && artistHit < 2)
						score -= 80;
				}

				qint64 candDur = maybeDurationMs(o);
				if (song.durationMs > 0 && candDur > 0)
				{
					qint64 diff = song.durationMs - candDur;
					if (diff < 0)
						diff = -diff;
					if (diff <= 2000)
						score += 60;
					else if (diff <= 5000)
						score += 40;
					else if (diff <= 10000)
						score += 15;
					else
						score -= 40;
				}

				if (!albumNorm.isEmpty() && !candAlbumNorm.isEmpty())
				{
					if (candAlbumNorm == albumNorm)
						score += 35;
					else if (candAlbumNorm.contains(albumNorm) || albumNorm.contains(candAlbumNorm))
						score += 20;
					else
						score -= 10;
				}

				QString raw = candTitle + QStringLiteral(" ") + candArtist + QStringLiteral(" ") + candAlbum;
				QSet<QString> candFlags = extractFlags(raw);
				for (const QString &flag : songFlags)
				{
					if (!candFlags.contains(flag))
						score -= 45;
				}
				for (const QString &flag : candFlags)
				{
					if (!songFlags.contains(flag))
						score -= 35;
				}
				return score;
			};

			const int soloScore = calcScore(arr.at(0).toObject());
			const int duetScore = calcScore(arr.at(1).toObject());
			Q_ASSERT(duetScore > soloScore);
			Q_ASSERT(soloScore < 200);
			Q_ASSERT(duetScore >= 200);
		}

		{
			Song song;
			song.name = QStringLiteral("布拉格广场");
			Artist a1;
			a1.name = QStringLiteral("蔡依林");
			Artist a2;
			a2.name = QStringLiteral("周杰伦");
			song.artists = {a1, a2};
			song.durationMs = 294600;

			QJsonObject coverCandidate{{QStringLiteral("id"), QStringLiteral("x")},
							 {QStringLiteral("name"), QStringLiteral("布拉格广场 (cover: 蔡依林|周杰伦)")},
							 {QStringLiteral("artist"), QStringLiteral("秋的呼吸Rita")},
							 {QStringLiteral("album"), QStringLiteral("秋的呼吸Rita翻唱集")},
							 {QStringLiteral("duration"), 294000}};

			auto normalizeText = [](QString s) -> QString {
				s = s.trimmed().toLower();
				s.replace('\\', QChar());
				static const QRegularExpression brackets(QStringLiteral("[\\(\\[\\{\\uff08\\u3010].*?[\\)\\]\\}\\uff09\\u3011]"));
				s.replace(brackets, QString());
				static const QRegularExpression nonWord(QStringLiteral("[^\\p{L}\\p{N}]+"));
				s.replace(nonWord, QString());
				return s;
			};
			auto extractFlags = [](const QString &raw) -> QSet<QString> {
				QSet<QString> flags;
				auto addIf = [&flags, &raw](const QString &key, const QStringList &tokens) {
					for (const QString &t : tokens)
					{
						if (raw.contains(t, Qt::CaseInsensitive))
						{
							flags.insert(key);
							return;
						}
					}
				};
				addIf(QStringLiteral("cover"), {QStringLiteral("cover"), QStringLiteral("翻唱")});
				addIf(QStringLiteral("inst"), {QStringLiteral("inst"), QStringLiteral("instrumental"), QStringLiteral("伴奏"), QStringLiteral("纯音乐")});
				return flags;
			};

			QStringList songArtistNorms;
			for (const Artist &a : song.artists)
			{
				QString an = normalizeText(a.name);
				if (!an.isEmpty() && !songArtistNorms.contains(an))
					songArtistNorms.append(an);
			}
			QString candTitle = coverCandidate.value(QStringLiteral("name")).toString();
			QString candArtist = coverCandidate.value(QStringLiteral("artist")).toString();
			QString candAlbum = coverCandidate.value(QStringLiteral("album")).toString();
			QString candArtistNorm = normalizeText(candArtist);
			QSet<QString> songFlags = extractFlags(song.name);
			QSet<QString> candFlags = extractFlags(candTitle + QStringLiteral(" ") + candArtist + QStringLiteral(" ") + candAlbum);
			int artistHit = 0;
			for (const QString &an : songArtistNorms)
			{
				if (!an.isEmpty() && candArtistNorm.contains(an))
					artistHit++;
			}
			bool acceptable = true;
			if (songArtistNorms.size() >= 2 && artistHit < 2)
				acceptable = false;
			if (!songFlags.contains(QStringLiteral("cover")) && candFlags.contains(QStringLiteral("cover")))
				acceptable = false;
			Q_ASSERT(!acceptable);
		}
	}
#endif
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
	// 保证整个 playUrl 流程只会回调一次：多策略兜底会有多条异步路径
	QSharedPointer<bool> finished = QSharedPointer<bool>::create(false);
	auto finish = [callback, finished](const Result<PlayUrl> &result) {
		if (*finished)
			return;
		*finished = true;
		callback(result);
	};

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
		const QStringList allowed = {QStringLiteral("migu"), QStringLiteral("kugou"), QStringLiteral("pyncmd"), QStringLiteral("bilibili")};
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

	auto startUnblockProcess = [this, songId, finish, token, readEnabledPlatforms, findMusicApiDir](const Song &song) {
		if (token->isCancelled())
		{
			Error e;
			e.category = ErrorCategory::Network;
			e.code = -2;
			e.message = QStringLiteral("Request cancelled");
			finish(Result<PlayUrl>::failure(e));
			return;
		}

		Logger::info(QStringLiteral("Start unblock process: songId=%1, name=%2")
					 .arg(songId)
					 .arg(song.name));

		QString apiDir = findMusicApiDir();
		if (apiDir.isEmpty())
		{
			Error e;
			e.category = ErrorCategory::UpstreamChange;
			e.code = -1;
			e.message = QStringLiteral("Embedded music API directory not found");
			finish(Result<PlayUrl>::failure(e));
			return;
		}

		QDir apiDirObj(apiDir);
		QString unblockPkg = apiDirObj.filePath(QStringLiteral("node_modules/@unblockneteasemusic/server/package.json"));
		if (!QFileInfo::exists(unblockPkg))
		{
			Error e;
			e.category = ErrorCategory::UpstreamChange;
			e.code = -1;
			e.message = QStringLiteral("Embedded music API dependencies not installed");
			e.detail = unblockPkg;
			finish(Result<PlayUrl>::failure(e));
			return;
		}

		QJsonObject songData;
		songData.insert(QStringLiteral("name"), song.name);
		songData.insert(QStringLiteral("duration"), static_cast<qint64>(song.durationMs));
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
			"const w=(...a)=>process.stderr.write(a.map(x=>typeof x==='string'?x:JSON.stringify(x)).join(' ')+'\\n');"
			"console.log=w;console.info=w;console.warn=w;console.error=w;"
			"(async()=>{"
			"const tail=process.argv.slice(-3);"
			"const id=parseInt(tail[0],10);"
			"let platforms;"
			"try{platforms=JSON.parse(tail[1]||'[]');}catch(e){platforms=[];}"
			"if(!Array.isArray(platforms)) platforms=[String(platforms)];"
			"const song=JSON.parse(tail[2]||'{}');"
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

		QObject::connect(proc, &QProcess::finished, proc, [proc, timer, finish](int exitCode, QProcess::ExitStatus exitStatus) {
			timer->stop();
			QByteArray out = proc->readAllStandardOutput();
			QByteArray err = proc->readAllStandardError();
			if (exitStatus != QProcess::NormalExit || exitCode != 0)
			{
				QString errTail = QString::fromLocal8Bit(err).right(800);
				Logger::warning(QStringLiteral("Unblock process failed: exitCode=%1, status=%2, stderr=%3")
								.arg(exitCode)
								.arg(static_cast<int>(exitStatus))
								.arg(errTail));
				Error e;
				e.category = ErrorCategory::Unknown;
				e.code = exitCode;
				e.message = QStringLiteral("Unblock music failed");
				e.detail = errTail;
				finish(Result<PlayUrl>::failure(e));
				proc->deleteLater();
				return;
			}

		QJsonObject o;
		QJsonDocument doc;
		QJsonParseError pe{};
		doc = QJsonDocument::fromJson(out, &pe);
		if (pe.error == QJsonParseError::NoError && doc.isObject())
			{
			o = doc.object();
			}
		else
		{
			QByteArray raw = out;
			int end = raw.lastIndexOf('}');
			while (end > 0)
			{
				int start = raw.lastIndexOf('{', end);
				if (start < 0)
					break;
				QByteArray candidate = raw.mid(start, end - start + 1);
				if (!candidate.contains("\"url\""))
				{
					end = raw.lastIndexOf('}', start - 1);
					continue;
				}
				QJsonParseError ce{};
				QJsonDocument cdoc = QJsonDocument::fromJson(candidate, &ce);
				if (ce.error == QJsonParseError::NoError && cdoc.isObject())
				{
					o = cdoc.object();
					break;
				}
				end = raw.lastIndexOf('}', start - 1);
			}
		}

		if (o.isEmpty())
		{
			Error e;
			e.category = ErrorCategory::Parser;
			e.code = -1;
			e.message = QStringLiteral("Parse unblock result failed");
			e.detail = QString::fromUtf8(out).right(800);
			finish(Result<PlayUrl>::failure(e));
			proc->deleteLater();
			return;
		}

		QString urlStr = o.value(QStringLiteral("url")).toString();
		urlStr = urlStr.trimmed();
		while (urlStr.startsWith('`') || urlStr.startsWith('"') || urlStr.startsWith('\''))
			urlStr.remove(0, 1);
		while (urlStr.endsWith('`') || urlStr.endsWith('"') || urlStr.endsWith('\''))
			urlStr.chop(1);
		urlStr = urlStr.trimmed();
		if (urlStr.isEmpty())
		{
			Error e;
			e.category = ErrorCategory::UpstreamChange;
			e.code = 404;
			e.message = QStringLiteral("Play url not found");
			finish(Result<PlayUrl>::failure(e));
			proc->deleteLater();
			return;
		}
		PlayUrl p;
		p.url = QUrl(urlStr);
		p.bitrate = o.value(QStringLiteral("br")).toInt();
		p.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
		finish(Result<PlayUrl>::success(p));
			proc->deleteLater();
		});

		proc->start();
	};

	auto tryGdMusicOrUnblock = [this, finish, token, cancelIfOuterCancelled, startUnblockProcess, finished](const Song &song) {
		if (token->isCancelled())
			return;
		if (*finished)
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
			Logger::info(QStringLiteral("Skip GD Studio search: query too short for song \"%1\"").arg(song.name));
			startUnblockProcess(song);
			return;
		}

		Logger::info(QStringLiteral("Try GD Studio search: \"%1\"").arg(searchQuery));

		const QUrl base(QStringLiteral("https://music-api.gdstudio.xyz/api.php"));
		const QStringList sources = {QStringLiteral("joox"), QStringLiteral("tidal"), QStringLiteral("netease")};

		struct State
		{
			int index = 0;
		};
		QSharedPointer<State> state = QSharedPointer<State>::create();
		QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
		*nextFn = [this, base, sources, searchQuery, finish, token, cancelIfOuterCancelled, startUnblockProcess, song, state, nextFn, finished]() {
			if (token->isCancelled())
				return;
			if (*finished)
				return;
			if (state->index >= sources.size())
			{
				Logger::warning(QStringLiteral("GD Studio search failed on all sources for \"%1\", fallback to unblock").arg(searchQuery));
				startUnblockProcess(song);
				return;
			}
			QString source = sources.at(state->index);
			state->index++;

			Logger::info(QStringLiteral("GD Studio search using source=%1, query=\"%2\"").arg(source, searchQuery));

			QUrl searchUrl = base;
			QUrlQuery q;
			q.addQueryItem(QStringLiteral("types"), QStringLiteral("search"));
			q.addQueryItem(QStringLiteral("source"), source);
			q.addQueryItem(QStringLiteral("name"), searchQuery);
			q.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
			q.addQueryItem(QStringLiteral("pages"), QStringLiteral("1"));
			searchUrl.setQuery(q);

			Logger::debug(QStringLiteral("GD Studio search url: %1").arg(searchUrl.toString(QUrl::FullyEncoded)));

			HttpRequestOptions searchOpts;
			searchOpts.url = searchUrl;
			searchOpts.timeoutMs = 5000;
			QSharedPointer<RequestToken> searchToken = client->sendWithRetry(searchOpts, 1, 300, [this, base, source, searchQuery, finish, token, cancelIfOuterCancelled, startUnblockProcess, song, nextFn, finished](Result<HttpResponse> searchResult) {
				if (token->isCancelled())
					return;
				if (*finished)
					return;
				if (!searchResult.ok)
				{
					Logger::warning(QStringLiteral("GD Studio search http failed on source=%1: %2")
									.arg(source)
									.arg(searchResult.error.message));
					(*nextFn)();
					return;
				}

				QJsonParseError pe{};
				QJsonDocument doc = QJsonDocument::fromJson(searchResult.value.body, &pe);
				if (pe.error != QJsonParseError::NoError || !doc.isArray())
				{
					Logger::warning(QStringLiteral("GD Studio search parse failed on source=%1: %2")
									.arg(source)
									.arg(QString::fromUtf8(searchResult.value.body).right(300)));
					(*nextFn)();
					return;
				}
				QJsonArray arr = doc.array();
				if (arr.isEmpty())
				{
					Logger::info(QStringLiteral("GD Studio search empty on source=%1").arg(source));
					(*nextFn)();
					return;
				}
			QJsonValue firstVal = arr.at(0);
			if (!firstVal.isObject())
			{
				Logger::warning(QStringLiteral("GD Studio search first result not object on source=%1").arg(source));
				(*nextFn)();
				return;
			}
			QJsonObject first = firstVal.toObject();
			QJsonValue idVal = first.value(QStringLiteral("id"));
			QString trackId;
			if (idVal.isString())
				trackId = idVal.toString().trimmed();
			else if (idVal.isDouble())
				trackId = QString::number(idVal.toDouble(), 'f', 0).trimmed();
			if (trackId.isEmpty())
			{
				Logger::warning(QStringLiteral("GD Studio search result missing id on source=%1").arg(source));
				(*nextFn)();
				return;
			}
			QJsonValue srcVal = first.value(QStringLiteral("source"));
			QString trackSource;
			if (srcVal.isString())
				trackSource = srcVal.toString().trimmed();
			if (trackSource.compare(QStringLiteral("qq"), Qt::CaseInsensitive) == 0)
				trackSource = QStringLiteral("tencent");
			if (trackSource.isEmpty())
			{
				trackSource = source;
				if (trackSource.compare(QStringLiteral("qq"), Qt::CaseInsensitive) == 0)
					trackSource = QStringLiteral("tencent");
			}

			Logger::info(QStringLiteral("GD Studio using trackId=%1, trackSource=%2 for \"%3\"")
						 .arg(trackId, trackSource, searchQuery));

			QUrl urlUrl = base;
			QUrlQuery uq;
			uq.addQueryItem(QStringLiteral("types"), QStringLiteral("url"));
			uq.addQueryItem(QStringLiteral("source"), trackSource);
			uq.addQueryItem(QStringLiteral("id"), trackId);
			uq.addQueryItem(QStringLiteral("br"), QStringLiteral("999"));
			urlUrl.setQuery(uq);

			Logger::debug(QStringLiteral("GD Studio url request: %1").arg(urlUrl.toString(QUrl::FullyEncoded)));

				HttpRequestOptions urlOpts;
				urlOpts.url = urlUrl;
				urlOpts.timeoutMs = 5000;
				QSharedPointer<RequestToken> urlToken = client->sendWithRetry(urlOpts, 1, 300, [finish, token, nextFn, finished](Result<HttpResponse> urlResult) {
					if (token->isCancelled())
						return;
					if (*finished)
						return;
					if (!urlResult.ok)
					{
						Logger::warning(QStringLiteral("GD Studio url http failed: %1").arg(urlResult.error.message));
						(*nextFn)();
						return;
					}
					QJsonParseError pe{};
					QJsonDocument doc = QJsonDocument::fromJson(urlResult.value.body, &pe);
					if (pe.error != QJsonParseError::NoError || !doc.isObject())
					{
						Logger::warning(QStringLiteral("GD Studio url parse failed: %1")
										.arg(QString::fromUtf8(urlResult.value.body).right(300)));
						(*nextFn)();
						return;
					}
				QJsonObject o = doc.object();
				QString urlStr = o.value(QStringLiteral("url")).toString();
				urlStr.replace('\\', QString());
				urlStr = urlStr.trimmed();
				if (urlStr.isEmpty())
				{
					Logger::info(QStringLiteral("GD Studio url empty, try next source"));
					(*nextFn)();
					return;
				}
				PlayUrl p;
				p.url = QUrl(urlStr);
				p.bitrate = o.value(QStringLiteral("br")).toVariant().toInt();
				p.size = static_cast<qint64>(o.value(QStringLiteral("size")).toVariant().toLongLong());
				Logger::info(QStringLiteral("GD Studio url resolved: %1, br=%2, size=%3")
							 .arg(p.url.toString(), QString::number(p.bitrate), QString::number(p.size)));
				finish(Result<PlayUrl>::success(p));
				});
				cancelIfOuterCancelled(urlToken);
			});
			cancelIfOuterCancelled(searchToken);
		};

		(*nextFn)();
	};


	// 兼容旧链路：不依赖「搜索/详情」信息，直接尝试用原 songId 向 GD Studio 取 url
	auto tryGdStudioDirectUrl = [this, songId, finish, token, cancelIfOuterCancelled, finished](const std::function<void()> &onFailed) {
		if (token->isCancelled())
			return;
		if (*finished)
			return;
		Logger::info(QStringLiteral("Try GD Studio direct url for songId=%1").arg(songId));
		QUrl base(QStringLiteral("https://music-api.gdstudio.xyz/api.php"));
		QUrl urlUrl = base;
		QUrlQuery uq;
		uq.addQueryItem(QStringLiteral("types"), QStringLiteral("url"));
		uq.addQueryItem(QStringLiteral("source"), QStringLiteral("netease"));
		uq.addQueryItem(QStringLiteral("id"), songId);
		uq.addQueryItem(QStringLiteral("br"), QStringLiteral("999"));
		urlUrl.setQuery(uq);

		Logger::debug(QStringLiteral("GD Studio direct url request: %1").arg(urlUrl.toString(QUrl::FullyEncoded)));

		HttpRequestOptions urlOpts;
		urlOpts.url = urlUrl;
		urlOpts.timeoutMs = 5000;
		QSharedPointer<RequestToken> urlToken = client->sendWithRetry(urlOpts, 1, 300, [finish, token, onFailed, finished](Result<HttpResponse> urlResult) {
			if (token->isCancelled())
				return;
			if (*finished)
				return;
			if (!urlResult.ok)
			{
				Logger::warning(QStringLiteral("GD Studio direct url http failed: %1").arg(urlResult.error.message));
				onFailed();
				return;
			}
			QJsonParseError pe{};
			QJsonDocument doc = QJsonDocument::fromJson(urlResult.value.body, &pe);
			if (pe.error != QJsonParseError::NoError || !doc.isObject())
			{
				Logger::warning(QStringLiteral("GD Studio direct url parse failed: %1")
								.arg(QString::fromUtf8(urlResult.value.body).right(300)));
				onFailed();
				return;
			}
			QJsonObject o = doc.object();
			QString urlStr = o.value(QStringLiteral("url")).toString();
			urlStr.replace('\\', QString());
			urlStr = urlStr.trimmed();
			if (urlStr.isEmpty())
			{
				Logger::info(QStringLiteral("GD Studio direct url empty, fallback to search"));
				onFailed();
				return;
			}
			PlayUrl p;
			p.url = QUrl(urlStr);
			p.bitrate = o.value(QStringLiteral("br")).toVariant().toInt();
			p.size = static_cast<qint64>(o.value(QStringLiteral("size")).toVariant().toLongLong());
			Logger::info(QStringLiteral("GD Studio direct url resolved: %1, br=%2, size=%3")
						 .arg(p.url.toString(), QString::number(p.bitrate), QString::number(p.size)));
			finish(Result<PlayUrl>::success(p));
		});
		cancelIfOuterCancelled(urlToken);
	};

	HttpRequestOptions v1;
	v1.url = buildUrl(QStringLiteral("/song/url/v1"), {{QStringLiteral("id"), songId}, {QStringLiteral("level"), readQualityLevel()}, {QStringLiteral("encodeType"), QStringLiteral("aac")}});
	// 记录最近一次“非解析类”失败原因，避免最终只返回笼统 404
	QSharedPointer<Error> lastError = QSharedPointer<Error>::create();
	QSharedPointer<RequestToken> v1Token = client->sendWithRetry(v1, 2, 500, [this, songId, finish, token, cancelIfOuterCancelled, isUnblockEnabled, tryGdStudioDirectUrl, tryGdMusicOrUnblock, lastError, finished](Result<HttpResponse> result) {
		if (token->isCancelled())
			return;
		if (*finished)
			return;
		if (result.ok)
		{
			Result<PlayUrl> parsed = parsePlayUrl(result.value.body);
			if (parsed.ok)
			{
				finish(parsed);
				return;
			}
			if (parsed.error.category != ErrorCategory::UpstreamChange)
			{
				finish(Result<PlayUrl>::failure(parsed.error));
				return;
			}
		}
		else
		{
			*lastError = result.error;
		}

		HttpRequestOptions legacy;
		legacy.url = buildUrl(QStringLiteral("/song/url"), {{QStringLiteral("id"), songId}, {QStringLiteral("br"), QStringLiteral("320000")}});
		QSharedPointer<RequestToken> legacyToken = client->sendWithRetry(legacy, 1, 500, [this, songId, finish, token, cancelIfOuterCancelled, isUnblockEnabled, tryGdStudioDirectUrl, tryGdMusicOrUnblock, lastError, finished](Result<HttpResponse> legacyResult) {
			if (token->isCancelled())
				return;
			if (*finished)
				return;
			if (legacyResult.ok)
			{
				Result<PlayUrl> parsed = parsePlayUrl(legacyResult.value.body);
				if (parsed.ok)
				{
					finish(parsed);
					return;
				}
				if (parsed.error.category != ErrorCategory::UpstreamChange)
				{
					finish(Result<PlayUrl>::failure(parsed.error));
					return;
				}
			}
			else
			{
				*lastError = legacyResult.error;
			}

			auto failByLastErrorOrNotFound = [finish, lastError]() {
				if (lastError && lastError->code != 0)
					finish(Result<PlayUrl>::failure(*lastError));
				else
				{
					Error e;
					e.category = ErrorCategory::UpstreamChange;
					e.code = 404;
					e.message = QStringLiteral("Play url not found");
					finish(Result<PlayUrl>::failure(e));
				}
			};

			if (!isUnblockEnabled())
			{
				failByLastErrorOrNotFound();
				return;
			}

			HttpRequestOptions detail;
			detail.url = buildUrl(QStringLiteral("/song/detail"), {{QStringLiteral("ids"), songId}});
			QSharedPointer<RequestToken> detailToken = client->sendWithRetry(detail, 1, 500, [this, finish, token, tryGdStudioDirectUrl, tryGdMusicOrUnblock, failByLastErrorOrNotFound, finished](Result<HttpResponse> detailResult) {
				if (token->isCancelled())
					return;
				if (*finished)
					return;
				if (!detailResult.ok)
				{
					failByLastErrorOrNotFound();
					return;
				}
				Result<Song> parsedSong = this->parseSongDetail(detailResult.value.body);
				if (!parsedSong.ok)
				{
					failByLastErrorOrNotFound();
					return;
				}
				Song song = parsedSong.value;
				tryGdStudioDirectUrl([tryGdMusicOrUnblock, song]() mutable {
					tryGdMusicOrUnblock(song);
				});
			});
			cancelIfOuterCancelled(detailToken);
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
	bool isTrial = false;
	int trialStart = 0;
	int trialEnd = 0;
	QJsonValue freeTrialInfo = o.value(QStringLiteral("freeTrialInfo"));
	if (freeTrialInfo.isObject())
	{
		QJsonObject ti = freeTrialInfo.toObject();
		trialStart = ti.value(QStringLiteral("start")).toInt();
		trialEnd = ti.value(QStringLiteral("end")).toInt();
		if (trialEnd > trialStart)
			isTrial = true;
	}
	QJsonValue freeTrialPrivilege = o.value(QStringLiteral("freeTrialPrivilege"));
	if (!isTrial && freeTrialPrivilege.isObject())
	{
		QJsonObject tp = freeTrialPrivilege.toObject();
		int listenType = tp.value(QStringLiteral("listenType")).toInt();
		if (listenType > 0)
			isTrial = true;
	}
	if (isTrial)
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 402;
		e.message = QStringLiteral("Official play url is trial");
		if (trialEnd > trialStart)
			e.detail = QStringLiteral("trial=%1-%2").arg(trialStart).arg(trialEnd);
		return Result<PlayUrl>::failure(e);
	}
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
