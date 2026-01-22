#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace App
{

struct Artist
{
	QString id;
	QString name;
};

struct Album
{
	QString id;
	QString name;
	QUrl coverUrl;
};

struct PlayUrl
{
	QUrl url;
	int bitrate = 0;
	qint64 size = 0;
};

struct LyricLine
{
	qint64 timeMs = 0;
	QString text;
};

struct Lyric
{
	QList<LyricLine> lines;
};

struct Song
{
	QString id;
	QString name;
	QList<Artist> artists;
	Album album;
	qint64 durationMs = 0;
	PlayUrl playUrl;
};

struct Playlist
{
	QString id;
	QString name;
	QList<Song> songs;
};

enum class ErrorCategory
{
	Network,
	Parser,
	Auth,
	UpstreamChange,
	RateLimit,
	Unknown
};

struct Error
{
	ErrorCategory category = ErrorCategory::Unknown;
	int code = 0;
	QString message;
	QString detail;
};

template <typename T>
struct Result
{
	bool ok = false;
	T value{};
	Error error;

	static Result<T> success(const T &v)
	{
		Result<T> r;
		r.ok = true;
		r.value = v;
		return r;
	}

	static Result<T> failure(const Error &e)
	{
		Result<T> r;
		r.ok = false;
		r.error = e;
		return r;
	}
};

}

