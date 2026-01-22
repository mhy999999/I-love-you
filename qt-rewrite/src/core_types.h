// 核心领域模型与通用结果类型定义
#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace App
{

// 艺术家信息
struct Artist
{
	QString id;
	QString name;
};

// 专辑信息
struct Album
{
	QString id;
	QString name;
	QUrl coverUrl;
};

// 播放地址信息（单个音频源）
struct PlayUrl
{
	QUrl url;
	int bitrate = 0;
	qint64 size = 0;
};

// 单行歌词（时间戳 + 文本）
struct LyricLine
{
	qint64 timeMs = 0;
	QString text;
};

// 完整歌词，由多行组成
struct Lyric
{
	QList<LyricLine> lines;
};

// 歌曲基础信息
struct Song
{
	QString id;
	QString name;
	QList<Artist> artists;
	Album album;
	qint64 durationMs = 0;
	PlayUrl playUrl;
};

// 歌单信息，包含一组歌曲
struct Playlist
{
	QString id;
	QString name;
	QList<Song> songs;
};

// 错误分类，用于统一错误上报
enum class ErrorCategory
{
	Network,
	Parser,
	Auth,
	UpstreamChange,
	RateLimit,
	Unknown
};

// 统一错误对象
struct Error
{
	ErrorCategory category = ErrorCategory::Unknown;
	int code = 0;
	QString message;
	QString detail;
};

// 泛型结果类型，用于携带返回值或错误信息
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
