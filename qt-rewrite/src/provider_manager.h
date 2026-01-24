// ProviderManager：负责 Provider 注册、选择与失败回退策略
#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

#include "core_types.h"
#include "provider.h"

namespace App
{

// Provider 管理器配置：包含全局顺序与 fallback 开关
struct ProviderManagerConfig
{
	// Provider 优先顺序，按 id 排列
	QStringList providerOrder;
	// 是否启用失败自动 fallback
	bool fallbackEnabled = true;
};

// ProviderManager：统一管理多个音乐来源并提供 fallback 调度
class ProviderManager : public QObject
{
	Q_OBJECT

public:
	// 构造函数，可指定父对象用于 Qt 生命周期管理
	explicit ProviderManager(QObject *parent = nullptr);

	// 注册一个 Provider，若 id 已存在则覆盖旧实例
	void registerProvider(IProvider *provider);
	// 按 id 获取 Provider，不存在时返回 nullptr
	IProvider *providerById(const QString &id) const;
	// 返回当前已注册的 Provider 列表
	QList<IProvider *> providers() const;

	// 设置全局 Provider 顺序与 fallback 策略
	void setConfig(const ProviderManagerConfig &config);
	// 获取当前配置快照
	ProviderManagerConfig config() const;

	// 搜索歌曲，按配置顺序与 fallback 策略选择 Provider
	QSharedPointer<RequestToken> search(const QString &keyword, int limit, int offset, const IProvider::SearchCallback &callback, const QStringList &preferredProviderIds = {});
	// 获取歌曲详情，支持多 Provider fallback
	QSharedPointer<RequestToken> songDetail(const QString &songId, const IProvider::SongDetailCallback &callback, const QStringList &preferredProviderIds = {});
	// 获取播放地址，支持多 Provider fallback
	QSharedPointer<RequestToken> playUrl(const QString &songId, const IProvider::PlayUrlCallback &callback, const QStringList &preferredProviderIds = {});
	// 获取歌词，支持多 Provider fallback
	QSharedPointer<RequestToken> lyric(const QString &songId, const IProvider::LyricCallback &callback, const QStringList &preferredProviderIds = {});
	// 获取封面图片内容，支持多 Provider fallback
	QSharedPointer<RequestToken> cover(const QUrl &coverUrl, const IProvider::CoverCallback &callback, const QStringList &preferredProviderIds = {});
	// 获取歌单详情，支持多 Provider fallback
	QSharedPointer<RequestToken> playlistDetail(const QString &playlistId, const IProvider::PlaylistDetailCallback &callback, const QStringList &preferredProviderIds = {});
	// 分页拉取歌单曲目，支持多 Provider fallback
	QSharedPointer<RequestToken> playlistTracks(const QString &playlistId, int limit, int offset, const IProvider::PlaylistTracksCallback &callback, const QStringList &preferredProviderIds = {});

private:
	// Provider 存储：id -> 实例指针
	QHash<QString, IProvider *> providerMap;
	// 管理器配置，包括顺序与 fallback 开关
	ProviderManagerConfig managerConfig;

	// 根据配置与能力筛选候选 Provider 列表
	QList<IProvider *> resolveProviders(const QStringList &preferredProviderIds, std::function<bool(IProvider *)> predicate) const;
	// 从 ProviderId 列表中去重并保留顺序
	QStringList normalizeOrder(const QStringList &order) const;
};

}
