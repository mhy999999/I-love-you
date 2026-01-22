// 音乐来源 Provider 接口定义：统一搜索、详情、播放地址与歌词等能力
#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QUrl>

#include <functional>

#include "core_types.h"

namespace App
{

// 前向声明请求取消令牌，供 Provider 接口复用
class RequestToken;

// Provider 接口：抽象单个音乐来源的能力
class IProvider : public QObject
{
	Q_OBJECT

public:
	// 构造函数，允许指定父对象用于生命周期托管
	explicit IProvider(QObject *parent = nullptr)
		: QObject(parent)
	{
	}

	// 虚析构函数，确保多态删除安全
	~IProvider() override = default;

	// Provider 唯一标识，例如 "netease" / "qq" / "kuwo"
	virtual QString id() const = 0;
	// Provider 展示名称，用于 UI 显示
	virtual QString displayName() const = 0;

	// 是否支持搜索能力，默认支持
	virtual bool supportsSearch() const { return true; }
	// 是否支持歌曲详情查询，默认支持
	virtual bool supportsSongDetail() const { return true; }
	// 是否支持播放地址获取，默认支持
	virtual bool supportsPlayUrl() const { return true; }
	// 是否支持歌词获取，默认不强制支持
	virtual bool supportsLyric() const { return false; }
	// 是否支持封面图片拉取（通常用于需要特殊 header/cookie 的来源）
	virtual bool supportsCover() const { return false; }

	// 搜索结果回调类型：返回歌曲列表或错误
	using SearchCallback = std::function<void(Result<QList<Song>>) >;
	// 歌曲详情回调类型：返回单曲信息或错误
	using SongDetailCallback = std::function<void(Result<Song>)>;
	// 播放地址回调类型：返回播放地址或错误
	using PlayUrlCallback = std::function<void(Result<PlayUrl>)>;
	// 歌词回调类型：返回歌词或错误
	using LyricCallback = std::function<void(Result<Lyric>)>;
	// 封面回调类型：返回图片二进制或错误
	using CoverCallback = std::function<void(Result<QByteArray>)>;

	// 按关键字搜索歌曲，limit 控制最大返回条数
	virtual QSharedPointer<RequestToken> search(const QString &keyword, int limit, const SearchCallback &callback) = 0;
	// 根据歌曲 id 获取完整详情
	virtual QSharedPointer<RequestToken> songDetail(const QString &songId, const SongDetailCallback &callback) = 0;
	// 根据歌曲 id 获取可播放地址
	virtual QSharedPointer<RequestToken> playUrl(const QString &songId, const PlayUrlCallback &callback) = 0;
	// 根据歌曲 id 获取歌词，若不支持可返回错误
	virtual QSharedPointer<RequestToken> lyric(const QString &songId, const LyricCallback &callback) = 0;
	// 拉取封面图片内容（以 coverUrl 为键，部分来源可能需要特定请求头）
	virtual QSharedPointer<RequestToken> cover(const QUrl &coverUrl, const CoverCallback &callback) = 0;
};

}

