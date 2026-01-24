// SongListModel：将 QList<Song> 暴露给 QML 的简单列表模型
#pragma once

#include <QAbstractListModel>

#include "core_types.h"

namespace App
{

class SongListModel : public QAbstractListModel
{
	Q_OBJECT

public:
	enum Roles
	{
		IdRole = Qt::UserRole + 1,
		ProviderIdRole,
		SourceRole,
		NameRole,
		ArtistsRole,
		AlbumRole,
			DurationRole,
			CoverUrlRole,
		IsLoadedRole
	};

	explicit SongListModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	void setSongs(const QList<Song> &songs);
	const QList<Song> &songs() const;
	Q_INVOKABLE QVariantMap get(int row) const;

	// 供控制器维护队列
    void append(const Song &song);
    void insert(int index, const Song &song);
    void removeAt(int index);
    void clear();
    Q_INVOKABLE void move(int from, int to);

    // 懒加载支持
    void setTotalCount(int count);
    void updateRange(int start, const QList<Song> &songs);
    void clearRange(int start, int count);
    bool isLoaded(int row) const;

signals:
    void rowRequested(int index);
    void itemMoved(int from, int to);

private:
	QList<Song> m_songs;
};

}
