// SongListModel 实现：为 QML 提供歌曲列表数据源
#include "song_list_model.h"

namespace App
{

SongListModel::SongListModel(QObject *parent)
	: QAbstractListModel(parent)
{
}

int SongListModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return m_songs.size();
}

QVariant SongListModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size())
		return {};
	const Song &s = m_songs.at(index.row());
	switch (role)
	{
	case IdRole:
		return s.id;
	case NameRole:
		return s.name;
	case ArtistsRole: {
		QStringList names;
		for (const Artist &a : s.artists)
			names.append(a.name);
		return names.join(QStringLiteral(" / "));
	}
	case AlbumRole:
		return s.album.name;
	case DurationRole:
		return static_cast<qint64>(s.durationMs);
	default:
		return {};
	}
}

QHash<int, QByteArray> SongListModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[IdRole] = "songId";
	roles[NameRole] = "title";
	roles[ArtistsRole] = "artists";
	roles[AlbumRole] = "album";
	roles[DurationRole] = "duration";
	return roles;
}

void SongListModel::setSongs(const QList<Song> &songs)
{
	beginResetModel();
	m_songs = songs;
	endResetModel();
}

const QList<Song> &SongListModel::songs() const
{
	return m_songs;
}

QVariantMap SongListModel::get(int row) const
{
	QVariantMap map;
	if (row < 0 || row >= m_songs.size())
		return map;
	const Song &s = m_songs.at(row);
	map.insert(QStringLiteral("songId"), s.id);
	map.insert(QStringLiteral("title"), s.name);
	QStringList names;
	for (const Artist &a : s.artists)
		names.append(a.name);
	map.insert(QStringLiteral("artists"), names.join(QStringLiteral(" / ")));
	map.insert(QStringLiteral("album"), s.album.name);
	map.insert(QStringLiteral("duration"), static_cast<qint64>(s.durationMs));
	return map;
}

}

