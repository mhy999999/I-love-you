// ProviderManager 实现：集中处理 Provider 选择与失败回退逻辑
#include "provider_manager.h"

#include "http_client.h"
#include "logger.h"

namespace App
{

// ProviderManager 构造函数，初始化默认配置
ProviderManager::ProviderManager(QObject *parent)
	: QObject(parent)
{
}

// 注册 Provider，按 id 写入映射表
void ProviderManager::registerProvider(IProvider *provider)
{
	if (!provider)
		return;
	providerMap.insert(provider->id(), provider);
	// 若 Provider 没有父对象，则由管理器接管生命周期
	if (!provider->parent())
		provider->setParent(this);
}

// 按 id 查询 Provider 实例
IProvider *ProviderManager::providerById(const QString &id) const
{
	return providerMap.value(id, nullptr);
}

// 返回所有已注册 Provider 列表
QList<IProvider *> ProviderManager::providers() const
{
	return providerMap.values();
}

// 设置新的配置并归一化顺序
void ProviderManager::setConfig(const ProviderManagerConfig &config)
{
	managerConfig = config;
	managerConfig.providerOrder = normalizeOrder(managerConfig.providerOrder);
}

// 返回当前配置的拷贝
ProviderManagerConfig ProviderManager::config() const
{
	return managerConfig;
}

// 解析 Provider 顺序并按谓词筛选
QList<IProvider *> ProviderManager::resolveProviders(const QStringList &preferredProviderIds, std::function<bool(IProvider *)> predicate) const
{
	QList<IProvider *> result;
	QStringList order;
	if (!preferredProviderIds.isEmpty())
		order = normalizeOrder(preferredProviderIds);
	else if (!managerConfig.providerOrder.isEmpty())
		order = managerConfig.providerOrder;
	else
		order = providerMap.keys();

	for (const QString &id : order)
	{
		IProvider *p = providerMap.value(id, nullptr);
		if (!p)
			continue;
		if (predicate && !predicate(p))
			continue;
		result.append(p);
	}
	return result;
}

// 去重并保留 id 顺序
QStringList ProviderManager::normalizeOrder(const QStringList &order) const
{
	QStringList normalized;
	for (const QString &id : order)
	{
		if (normalized.contains(id))
			continue;
		if (!providerMap.contains(id))
			continue;
		normalized.append(id);
	}
	return normalized;
}

// 搜索歌曲，按顺序尝试多个 Provider 并在失败时自动 fallback
QSharedPointer<RequestToken> ProviderManager::search(const QString &keyword, int limit, const IProvider::SearchCallback &callback, const QStringList &preferredProviderIds)
{
	QList<IProvider *> candidates = resolveProviders(preferredProviderIds, [](IProvider *p) { return p->supportsSearch(); });
	if (candidates.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("No providers available for search");
		callback(Result<QList<Song>>::failure(e));
		return {};
	}

	QSharedPointer<RequestToken> masterToken = QSharedPointer<RequestToken>::create();
	struct State
	{
		int index = 0;
		QSharedPointer<RequestToken> currentToken;
		Error lastError;
	};
	QSharedPointer<State> state = QSharedPointer<State>::create();
	QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
	*nextFn = [this, keyword, limit, candidates, callback, masterToken, state, nextFn]() {
		// 若外层已取消，则不再继续
		if (masterToken->isCancelled())
		{
			if (state->currentToken)
				state->currentToken->cancel();
			return;
		}
		// 已经尝试完所有 Provider，则返回最后一次错误
		if (state->index >= candidates.size())
		{
			callback(Result<QList<Song>>::failure(state->lastError));
			return;
		}
		IProvider *provider = candidates.at(state->index);
		state->currentToken = provider->search(keyword, limit, [this, callback, masterToken, state, nextFn, candidates](Result<QList<Song>> result) {
			if (masterToken->isCancelled())
				return;
			// 成功或未启用 fallback 时直接返回
			if (result.ok || !managerConfig.fallbackEnabled || state->index >= candidates.size() - 1)
			{
				callback(result);
				return;
			}
			// 记录错误并尝试下一个 Provider
			state->lastError = result.error;
			state->index++;
			(*nextFn)();
		});
		// 将外层取消与当前请求绑定
		QObject::connect(masterToken.data(), &RequestToken::cancelled, provider, [state]() {
			if (state->currentToken)
				state->currentToken->cancel();
		});
	};
	(*nextFn)();
	return masterToken;
}

// 获取歌曲详情，支持多 Provider fallback
QSharedPointer<RequestToken> ProviderManager::songDetail(const QString &songId, const IProvider::SongDetailCallback &callback, const QStringList &preferredProviderIds)
{
	QList<IProvider *> candidates = resolveProviders(preferredProviderIds, [](IProvider *p) { return p->supportsSongDetail(); });
	if (candidates.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("No providers available for song detail");
		callback(Result<Song>::failure(e));
		return {};
	}

	QSharedPointer<RequestToken> masterToken = QSharedPointer<RequestToken>::create();
	struct State
	{
		int index = 0;
		QSharedPointer<RequestToken> currentToken;
		Error lastError;
	};
	QSharedPointer<State> state = QSharedPointer<State>::create();
	QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
	*nextFn = [this, songId, candidates, callback, masterToken, state, nextFn]() {
		if (masterToken->isCancelled())
		{
			if (state->currentToken)
				state->currentToken->cancel();
			return;
		}
		if (state->index >= candidates.size())
		{
			callback(Result<Song>::failure(state->lastError));
			return;
		}
		IProvider *provider = candidates.at(state->index);
		state->currentToken = provider->songDetail(songId, [this, callback, masterToken, state, nextFn, candidates](Result<Song> result) {
			if (masterToken->isCancelled())
				return;
			if (result.ok || !managerConfig.fallbackEnabled || state->index >= candidates.size() - 1)
			{
				callback(result);
				return;
			}
			state->lastError = result.error;
			state->index++;
			(*nextFn)();
		});
		QObject::connect(masterToken.data(), &RequestToken::cancelled, provider, [state]() {
			if (state->currentToken)
				state->currentToken->cancel();
		});
	};
	(*nextFn)();
	return masterToken;
}

// 获取播放地址，支持多 Provider fallback
QSharedPointer<RequestToken> ProviderManager::playUrl(const QString &songId, const IProvider::PlayUrlCallback &callback, const QStringList &preferredProviderIds)
{
	QList<IProvider *> candidates = resolveProviders(preferredProviderIds, [](IProvider *p) { return p->supportsPlayUrl(); });
	if (candidates.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("No providers available for playUrl");
		callback(Result<PlayUrl>::failure(e));
		return {};
	}

	QSharedPointer<RequestToken> masterToken = QSharedPointer<RequestToken>::create();
	struct State
	{
		int index = 0;
		QSharedPointer<RequestToken> currentToken;
		Error lastError;
	};
	QSharedPointer<State> state = QSharedPointer<State>::create();
	QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
	*nextFn = [this, songId, candidates, callback, masterToken, state, nextFn]() {
		if (masterToken->isCancelled())
		{
			if (state->currentToken)
				state->currentToken->cancel();
			return;
		}
		if (state->index >= candidates.size())
		{
			callback(Result<PlayUrl>::failure(state->lastError));
			return;
		}
		IProvider *provider = candidates.at(state->index);
		state->currentToken = provider->playUrl(songId, [this, callback, masterToken, state, nextFn, candidates](Result<PlayUrl> result) {
			if (masterToken->isCancelled())
				return;
			if (result.ok || !managerConfig.fallbackEnabled || state->index >= candidates.size() - 1)
			{
				callback(result);
				return;
			}
			state->lastError = result.error;
			state->index++;
			(*nextFn)();
		});
		QObject::connect(masterToken.data(), &RequestToken::cancelled, provider, [state]() {
			if (state->currentToken)
				state->currentToken->cancel();
		});
	};
	(*nextFn)();
	return masterToken;
}

// 获取歌词，支持多 Provider fallback
QSharedPointer<RequestToken> ProviderManager::lyric(const QString &songId, const IProvider::LyricCallback &callback, const QStringList &preferredProviderIds)
{
	QList<IProvider *> candidates = resolveProviders(preferredProviderIds, [](IProvider *p) { return p->supportsLyric(); });
	if (candidates.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("No providers available for lyric");
		callback(Result<Lyric>::failure(e));
		return {};
	}

	QSharedPointer<RequestToken> masterToken = QSharedPointer<RequestToken>::create();
	struct State
	{
		int index = 0;
		QSharedPointer<RequestToken> currentToken;
		Error lastError;
	};
	QSharedPointer<State> state = QSharedPointer<State>::create();
	QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
	*nextFn = [this, songId, candidates, callback, masterToken, state, nextFn]() {
		if (masterToken->isCancelled())
		{
			if (state->currentToken)
				state->currentToken->cancel();
			return;
		}
		if (state->index >= candidates.size())
		{
			callback(Result<Lyric>::failure(state->lastError));
			return;
		}
		IProvider *provider = candidates.at(state->index);
		state->currentToken = provider->lyric(songId, [this, callback, masterToken, state, nextFn, candidates](Result<Lyric> result) {
			if (masterToken->isCancelled())
				return;
			if (result.ok || !managerConfig.fallbackEnabled || state->index >= candidates.size() - 1)
			{
				callback(result);
				return;
			}
			state->lastError = result.error;
			state->index++;
			(*nextFn)();
		});
		QObject::connect(masterToken.data(), &RequestToken::cancelled, provider, [state]() {
			if (state->currentToken)
				state->currentToken->cancel();
		});
	};
	(*nextFn)();
	return masterToken;
}

QSharedPointer<RequestToken> ProviderManager::cover(const QUrl &coverUrl, const IProvider::CoverCallback &callback, const QStringList &preferredProviderIds)
{
	QList<IProvider *> candidates = resolveProviders(preferredProviderIds, [](IProvider *p) { return p->supportsCover(); });
	if (candidates.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("No providers available for cover");
		callback(Result<QByteArray>::failure(e));
		return {};
	}

	QSharedPointer<RequestToken> masterToken = QSharedPointer<RequestToken>::create();
	struct State
	{
		int index = 0;
		QSharedPointer<RequestToken> currentToken;
		Error lastError;
	};
	QSharedPointer<State> state = QSharedPointer<State>::create();
	QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
	*nextFn = [this, coverUrl, candidates, callback, masterToken, state, nextFn]() {
		if (masterToken->isCancelled())
		{
			if (state->currentToken)
				state->currentToken->cancel();
			return;
		}
		if (state->index >= candidates.size())
		{
			callback(Result<QByteArray>::failure(state->lastError));
			return;
		}
		IProvider *provider = candidates.at(state->index);
		state->currentToken = provider->cover(coverUrl, [this, callback, masterToken, state, nextFn, candidates](Result<QByteArray> result) {
			if (masterToken->isCancelled())
				return;
			if (result.ok || !managerConfig.fallbackEnabled || state->index >= candidates.size() - 1)
			{
				callback(result);
				return;
			}
			state->lastError = result.error;
			state->index++;
			(*nextFn)();
		});
		QObject::connect(masterToken.data(), &RequestToken::cancelled, provider, [state]() {
			if (state->currentToken)
				state->currentToken->cancel();
		});
	};
	(*nextFn)();
	return masterToken;
}

QSharedPointer<RequestToken> ProviderManager::playlistDetail(const QString &playlistId, const IProvider::PlaylistDetailCallback &callback, const QStringList &preferredProviderIds)
{
	QList<IProvider *> candidates = resolveProviders(preferredProviderIds, [](IProvider *p) { return p->supportsPlaylistDetail(); });
	if (candidates.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("No providers available for playlist detail");
		callback(Result<PlaylistMeta>::failure(e));
		return {};
	}

	QSharedPointer<RequestToken> masterToken = QSharedPointer<RequestToken>::create();
	struct State
	{
		int index = 0;
		QSharedPointer<RequestToken> currentToken;
		Error lastError;
	};
	QSharedPointer<State> state = QSharedPointer<State>::create();
	QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
	*nextFn = [this, playlistId, candidates, callback, masterToken, state, nextFn]() {
		if (masterToken->isCancelled())
		{
			if (state->currentToken)
				state->currentToken->cancel();
			return;
		}
		if (state->index >= candidates.size())
		{
			callback(Result<PlaylistMeta>::failure(state->lastError));
			return;
		}
		IProvider *provider = candidates.at(state->index);
		state->currentToken = provider->playlistDetail(playlistId, [this, callback, masterToken, state, nextFn, candidates](Result<PlaylistMeta> result) {
			if (masterToken->isCancelled())
				return;
			if (result.ok || !managerConfig.fallbackEnabled || state->index >= candidates.size() - 1)
			{
				callback(result);
				return;
			}
			state->lastError = result.error;
			state->index++;
			(*nextFn)();
		});
		QObject::connect(masterToken.data(), &RequestToken::cancelled, provider, [state]() {
			if (state->currentToken)
				state->currentToken->cancel();
		});
	};
	(*nextFn)();
	return masterToken;
}

QSharedPointer<RequestToken> ProviderManager::playlistTracks(const QString &playlistId, int limit, int offset, const IProvider::PlaylistTracksCallback &callback, const QStringList &preferredProviderIds)
{
	QList<IProvider *> candidates = resolveProviders(preferredProviderIds, [](IProvider *p) { return p->supportsPlaylistTracks(); });
	if (candidates.isEmpty())
	{
		Error e;
		e.category = ErrorCategory::UpstreamChange;
		e.code = 404;
		e.message = QStringLiteral("No providers available for playlist tracks");
		callback(Result<PlaylistTracksPage>::failure(e));
		return {};
	}

	QSharedPointer<RequestToken> masterToken = QSharedPointer<RequestToken>::create();
	struct State
	{
		int index = 0;
		QSharedPointer<RequestToken> currentToken;
		Error lastError;
	};
	QSharedPointer<State> state = QSharedPointer<State>::create();
	QSharedPointer<std::function<void()>> nextFn = QSharedPointer<std::function<void()>>::create();
	*nextFn = [this, playlistId, limit, offset, candidates, callback, masterToken, state, nextFn]() {
		if (masterToken->isCancelled())
		{
			if (state->currentToken)
				state->currentToken->cancel();
			return;
		}
		if (state->index >= candidates.size())
		{
			callback(Result<PlaylistTracksPage>::failure(state->lastError));
			return;
		}
		IProvider *provider = candidates.at(state->index);
		state->currentToken = provider->playlistTracks(playlistId, limit, offset, [this, callback, masterToken, state, nextFn, candidates](Result<PlaylistTracksPage> result) {
			if (masterToken->isCancelled())
				return;
			if (result.ok || !managerConfig.fallbackEnabled || state->index >= candidates.size() - 1)
			{
				callback(result);
				return;
			}
			state->lastError = result.error;
			state->index++;
			(*nextFn)();
		});
		QObject::connect(masterToken.data(), &RequestToken::cancelled, provider, [state]() {
			if (state->currentToken)
				state->currentToken->cancel();
		});
	};
	(*nextFn)();
	return masterToken;
}

}
