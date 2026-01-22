// MusicController：封装搜索与播放逻辑，供 QML 调用
#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include "core_types.h"
#include "http_client.h"
#include "netease_provider.h"
#include "provider_manager.h"
#include "song_list_model.h"

namespace App
{

class MusicController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(SongListModel *songsModel READ songsModel CONSTANT)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
	Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
	Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
	explicit MusicController(QObject *parent = nullptr);

	SongListModel *songsModel();
	bool loading() const;
	QUrl currentUrl() const;
	bool playing() const;
	int volume() const;
	void setVolume(int v);

	Q_INVOKABLE void search(const QString &keyword);
	Q_INVOKABLE void playIndex(int index);
	Q_INVOKABLE void pause();
	Q_INVOKABLE void resume();
	Q_INVOKABLE void stop();

signals:
	void loadingChanged();
	void currentUrlChanged();
	void playingChanged();
	void volumeChanged();
	void errorOccurred(const QString &message);

private:
	HttpClient httpClient;
	ProviderManager providerManager;
	NeteaseProvider *neteaseProvider = nullptr;
	SongListModel m_songsModel;
	QMediaPlayer m_player;
	QSharedPointer<RequestToken> searchToken;
	QSharedPointer<RequestToken> playUrlToken;
	bool m_loading = false;
	QUrl m_currentUrl;
	bool m_playing = false;

	void setLoading(bool v);
	void setCurrentUrl(const QUrl &url);
	void setPlaying(bool v);
};

}

