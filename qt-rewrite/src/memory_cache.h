// 通用内存缓存模板，带 TTL 过期与容量控制
#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

namespace App
{

template <typename T>
class MemoryCache
{
public:
	// 构造函数，指定最大条目数与默认过期时间（毫秒）
	explicit MemoryCache(int maxEntries = 256, int defaultTtlMs = 30000)
		: maxSize(maxEntries)
		, defaultTtl(defaultTtlMs)
	{
	}

	// 写入缓存条目，ttlMs <= 0 时使用默认 TTL
	void set(const QString &key, const T &value, int ttlMs = -1)
	{
		qint64 now = QDateTime::currentMSecsSinceEpoch();
		qint64 expire = now + static_cast<qint64>(ttlMs > 0 ? ttlMs : defaultTtl);
		Entry entry;
		entry.value = value;
		entry.expireAt = expire;
		entries.insert(key, entry);
		// 若超出容量，优先清理过期条目
		if (entries.size() > maxSize)
			evictExpired();
		// 若仍然超出容量，简单移除一条（近似 LRU，可按需扩展）
		if (entries.size() > maxSize)
		{
			auto it = entries.begin();
			if (it != entries.end())
				entries.erase(it);
		}
	}

	// 读取缓存条目，如过期或不存在返回 false
	bool get(const QString &key, T &out)
	{
		qint64 now = QDateTime::currentMSecsSinceEpoch();
		auto it = entries.find(key);
		if (it == entries.end())
			return false;
		if (it->expireAt < now)
		{
			entries.erase(it);
			return false;
		}
		out = it->value;
		return true;
	}

	// 移除指定 key 的缓存
	void remove(const QString &key)
	{
		entries.remove(key);
	}

	// 清空所有缓存
	void clear()
	{
		entries.clear();
	}

private:
	// 缓存内部条目结构
	struct Entry
	{
		T value;
		qint64 expireAt = 0;
	};

	// 实际存储结构：key -> 条目
	QHash<QString, Entry> entries;
	// 最大条目数
	int maxSize;
	// 默认过期时间（毫秒）
	int defaultTtl;

	// 清理已过期的缓存条目
	void evictExpired()
	{
		qint64 now = QDateTime::currentMSecsSinceEpoch();
		for (auto it = entries.begin(); it != entries.end();) 
		{
			if (it->expireAt < now)
				it = entries.erase(it);
			else
				++it;
		}
	}
};

}
