#pragma once

#include <QByteArray>
#include <QDir>
#include <QObject>
#include <QString>
#include <QStringList>
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
	QString filePathForKeyExt(const QString &key, const QString &ext) const;
	QUrl fileUrlForKeyExt(const QString &key, const QString &ext) const;
	QString resolveExistingFilePathForKey(const QString &key, const QStringList &exts) const;
	QUrl resolveExistingFileUrlForKey(const QString &key, const QStringList &exts) const;

	bool contains(const QString &key) const;
	bool containsAny(const QString &key, const QStringList &exts) const;
	bool get(const QString &key, QByteArray &outData);
	bool put(const QString &key, const QByteArray &data);
	bool putWithExt(const QString &key, const QByteArray &data, const QString &ext);
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
