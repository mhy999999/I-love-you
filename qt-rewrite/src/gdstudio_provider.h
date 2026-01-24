#pragma once

#include <QObject>

#include "http_client.h"
#include "provider.h"

namespace App
{

class GdStudioProvider : public IProvider
{
	Q_OBJECT

public:
	explicit GdStudioProvider(HttpClient *httpClient, QObject *parent = nullptr);

	QString id() const override;
	QString displayName() const override;
	bool supportsSongDetail() const override;
	bool supportsLyric() const override;
	bool supportsCover() const override;
	bool supportsPlaylistDetail() const override;
	bool supportsPlaylistTracks() const override;

	QSharedPointer<RequestToken> search(const QString &keyword, int limit, int offset, const SearchCallback &callback) override;
	QSharedPointer<RequestToken> songDetail(const QString &songId, const SongDetailCallback &callback) override;
	QSharedPointer<RequestToken> playUrl(const QString &songId, const PlayUrlCallback &callback) override;
	QSharedPointer<RequestToken> lyric(const QString &songId, const LyricCallback &callback) override;
	QSharedPointer<RequestToken> cover(const QUrl &coverUrl, const CoverCallback &callback) override;
	QSharedPointer<RequestToken> playlistDetail(const QString &playlistId, const PlaylistDetailCallback &callback) override;
	QSharedPointer<RequestToken> playlistTracks(const QString &playlistId, int limit, int offset, const PlaylistTracksCallback &callback) override;

private:
	HttpClient *client;
	QUrl apiBase;

	Result<QList<Song>> parseSearch(const QByteArray &body, int limit) const;
	Result<PlayUrl> parsePlayUrl(const QByteArray &body) const;
};

}

