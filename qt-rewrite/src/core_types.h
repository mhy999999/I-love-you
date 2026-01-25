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
	QString providerId;
	QString source;
	QString id;
	QString name;
	QList<Artist> artists;
	Album album;
	qint64 durationMs = 0;
	PlayUrl playUrl;
};

struct PlaylistMeta
{
	QString id;
	QString name;
	QUrl coverUrl;
	QString description;
	int trackCount = 0;
	QString creatorId;
    
    // Detailed info
    QStringList tags;
    bool subscribed = false;
    qint64 createTime = 0;
    qint64 updateTime = 0;
    qint64 playCount = 0;
    qint64 subscribedCount = 0;
    qint64 shareCount = 0;
    int commentCount = 0;
    
    // Creator info
    QString creatorName;
    QUrl creatorAvatar;
};

struct PlaylistTracksPage
{
	QString playlistId;
	QList<Song> songs;
	int total = 0;
	int offset = 0;
	int limit = 0;
};

struct UserProfile
{
	QString userId;
	QString nickname;
	QUrl avatarUrl;
	QString signature;
	int vipType = 0;
	QString cookie;
};

struct LoginQrKey
{
	QString unikey;
};

struct LoginQrCreate
{
	QString qrImg;
	QString qrUrl;
};

struct LoginQrCheck
{
	int code; // 800=expire, 801=waiting, 802=confirming, 803=success
	QString message;
	QString cookie;
};

struct HotSearchItem
{
    QString searchWord;
    QString content;
    int score = 0;
    int source = 0;
    int iconType = 0;
    QString iconUrl;
};

// 错误分类，用于统一错误上报
enum class ErrorCategory
{
	Network,
	Parser,
	Auth,
	UpstreamChange,
	RateLimit,
	Cancelled,
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
