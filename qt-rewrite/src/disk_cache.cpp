#include "disk_cache.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace App
{

DiskCache::DiskCache(const QString &namespaceName, qint64 maxBytes)
	: ns(namespaceName.trimmed())
	, maxSizeBytes(maxBytes)
{
}

QString DiskCache::rootDirPath() const
{
	QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	QDir dir(base);
	return dir.filePath(QStringLiteral("cache"));
}

QString DiskCache::ensureDir() const
{
	QString root = rootDirPath();
	QDir rootDir(root);
	if (!rootDir.exists())
		QDir().mkpath(root);
	QString sub = rootDir.filePath(ns.isEmpty() ? QStringLiteral("default") : ns);
	if (!QDir(sub).exists())
		QDir().mkpath(sub);
	return sub;
}

QString DiskCache::sha1Hex(const QString &s)
{
	QByteArray bytes = s.toUtf8();
	QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);
	return QString::fromLatin1(hash.toHex());
}

QString DiskCache::filePathForKey(const QString &key) const
{
	QString dir = ensureDir();
	QString name = sha1Hex(key);
	return QDir(dir).filePath(name + QStringLiteral(".bin"));
}

QUrl DiskCache::fileUrlForKey(const QString &key) const
{
	return QUrl::fromLocalFile(filePathForKey(key));
}

QString DiskCache::filePathForKeyExt(const QString &key, const QString &ext) const
{
	QString dir = ensureDir();
	QString name = sha1Hex(key);
	QString e = ext.trimmed().toLower();
	if (e.isEmpty())
		e = QStringLiteral("bin");
	return QDir(dir).filePath(name + QStringLiteral(".") + e);
}

QUrl DiskCache::fileUrlForKeyExt(const QString &key, const QString &ext) const
{
	return QUrl::fromLocalFile(filePathForKeyExt(key, ext));
}

bool DiskCache::contains(const QString &key) const
{
	return QFileInfo::exists(filePathForKey(key));
}

bool DiskCache::containsAny(const QString &key, const QStringList &exts) const
{
	for (const QString &e : exts)
	{
		QString path = filePathForKeyExt(key, e);
		if (QFileInfo::exists(path))
			return true;
	}
	return false;
}

void DiskCache::touch(const QString &filePath)
{
	QFile f(filePath);
	if (!f.exists())
		return;
	QDateTime now = QDateTime::currentDateTime();
	QFileDevice::FileTime when = QFileDevice::FileModificationTime;
	f.setFileTime(now, when);
}

bool DiskCache::get(const QString &key, QByteArray &outData)
{
	QString path = filePathForKey(key);
	QFile f(path);
	if (!f.exists())
		return false;
	if (!f.open(QIODevice::ReadOnly))
		return false;
	outData = f.readAll();
	f.close();
	touch(path);
	return true;
}

bool DiskCache::put(const QString &key, const QByteArray &data)
{
	QString path = filePathForKey(key);
	QFile f(path);
	QDir().mkpath(QFileInfo(path).absolutePath());
	if (!f.open(QIODevice::WriteOnly))
		return false;
	f.write(data);
	f.close();
	touch(path);
	prune();
	return true;
}

bool DiskCache::putWithExt(const QString &key, const QByteArray &data, const QString &ext)
{
	QString path = filePathForKeyExt(key, ext);
	QFile f(path);
	QDir().mkpath(QFileInfo(path).absolutePath());
	if (!f.open(QIODevice::WriteOnly))
		return false;
	f.write(data);
	f.close();
	touch(path);
	prune();
	return true;
}

qint64 DiskCache::computeTotalBytes(QList<QFileInfo> &outFiles) const
{
	QString dir = ensureDir();
	QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	qint64 total = 0;
	while (it.hasNext())
	{
		it.next();
		QFileInfo info = it.fileInfo();
		outFiles.append(info);
		total += info.size();
	}
	return total;
}

void DiskCache::prune()
{
	if (maxSizeBytes <= 0)
		return;
	QList<QFileInfo> files;
	qint64 total = computeTotalBytes(files);
	if (total <= maxSizeBytes)
		return;
	std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
		return a.lastModified() < b.lastModified();
	});
	for (const QFileInfo &fi : files)
	{
		if (total <= maxSizeBytes)
			break;
		qint64 size = fi.size();
		QFile::remove(fi.absoluteFilePath());
		total -= size;
	}
}

QString DiskCache::resolveExistingFilePathForKey(const QString &key, const QStringList &exts) const
{
	for (const QString &e : exts)
	{
		QString p = filePathForKeyExt(key, e);
		if (QFileInfo::exists(p))
			return p;
	}
	QString binPath = filePathForKey(key);
	if (QFileInfo::exists(binPath))
		return binPath;
	return QString();
}

QUrl DiskCache::resolveExistingFileUrlForKey(const QString &key, const QStringList &exts) const
{
	QString p = resolveExistingFilePathForKey(key, exts);
	if (p.isEmpty())
		return QUrl();
	return QUrl::fromLocalFile(p);
}

}
