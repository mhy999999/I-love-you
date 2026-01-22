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
		NameRole,
		ArtistsRole,
		AlbumRole,
		DurationRole
	};

	explicit SongListModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	void setSongs(const QList<Song> &songs);
	const QList<Song> &songs() const;
	Q_INVOKABLE QVariantMap get(int row) const;

private:
	QList<Song> m_songs;
};

}

