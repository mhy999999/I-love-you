#pragma once

#include <QByteArray>
#include <QDir>
#include <QObject>
#include <QString>
#include <QUrl>

namespace App
{

class DiskCache
{
public:
	explicit DiskCache(const QString &namespaceName, qint64 maxBytes);

	QString rootDirPath() const;
	QString filePathForKey(const QString &key) const;
	QUrl fileUrlForKey(const QString &key) const;

	bool contains(const QString &key) const;
	bool get(const QString &key, QByteArray &outData);
	bool put(const QString &key, const QByteArray &data);
	void prune();

private:
	QString ns;
	qint64 maxSizeBytes = 0;

	QString ensureDir() const;
	static QString sha1Hex(const QString &s);
	void touch(const QString &filePath);
	qint64 computeTotalBytes(QList<QFileInfo> &outFiles) const;
};

}
