// NeteaseProvider：基于本地 netease-cloud-music-api 的网易云 Provider 实现
#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include "core_types.h"
#include "http_client.h"
#include "provider.h"

namespace App
{

// 网易云 Provider，依赖本地运行的 netease-cloud-music-api 服务
class NeteaseProvider : public IProvider
{
	Q_OBJECT

public:
	explicit NeteaseProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent = nullptr);

	// IProvider 接口实现
	QString id() const override;
	QString displayName() const override;
	bool supportsLyric() const override;

	QSharedPointer<RequestToken> search(const QString &keyword, int limit, const SearchCallback &callback) override;
	QSharedPointer<RequestToken> songDetail(const QString &songId, const SongDetailCallback &callback) override;
	QSharedPointer<RequestToken> playUrl(const QString &songId, const PlayUrlCallback &callback) override;
	QSharedPointer<RequestToken> lyric(const QString &songId, const LyricCallback &callback) override;

private:
	HttpClient *client;
	QUrl apiBase;

	QUrl buildUrl(const QString &path, const QList<QPair<QString, QString>> &query) const;
	Result<QList<Song>> parseSearchSongs(const QByteArray &body) const;
	Result<Song> parseSongDetail(const QByteArray &body) const;
	Result<PlayUrl> parsePlayUrl(const QByteArray &body) const;
	Result<Lyric> parseLyric(const QByteArray &body) const;
};

}

