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
	explicit MemoryCache(int maxEntries = 256, int defaultTtlMs = 30000)
		: maxSize(maxEntries)
		, defaultTtl(defaultTtlMs)
	{
	}

	void set(const QString &key, const T &value, int ttlMs = -1)
	{
		qint64 now = QDateTime::currentMSecsSinceEpoch();
		qint64 expire = now + static_cast<qint64>(ttlMs > 0 ? ttlMs : defaultTtl);
		Entry entry;
		entry.value = value;
		entry.expireAt = expire;
		entries.insert(key, entry);
		if (entries.size() > maxSize)
			evictExpired();
		if (entries.size() > maxSize)
		{
			auto it = entries.begin();
			if (it != entries.end())
				entries.erase(it);
		}
	}

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

	void remove(const QString &key)
	{
		entries.remove(key);
	}

	void clear()
	{
		entries.clear();
	}

private:
	struct Entry
	{
		T value;
		qint64 expireAt = 0;
	};

	QHash<QString, Entry> entries;
	int maxSize;
	int defaultTtl;

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

