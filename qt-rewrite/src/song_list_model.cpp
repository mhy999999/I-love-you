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
	
	// Lazy load check
	if (s.id.isEmpty()) {
		const_cast<SongListModel*>(this)->rowRequested(index.row());
		if (role == NameRole) return QStringLiteral("Loading...");
		if (role == IsLoadedRole) return false;
		if (role == ArtistsRole || role == AlbumRole || role == CoverUrlRole) return QString();
		if (role == DurationRole) return 0;
		return {};
	}

	switch (role)
	{
	case IdRole:
		return s.id;
	case ProviderIdRole:
		return s.providerId;
	case SourceRole:
		return s.source;
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
	case CoverUrlRole:
		return s.album.coverUrl;
	case IsLoadedRole:
		return !s.id.isEmpty();
	default:
		return {};
	}
}

QHash<int, QByteArray> SongListModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[IdRole] = "songId";
	roles[ProviderIdRole] = "providerId";
	roles[SourceRole] = "source";
	roles[NameRole] = "title";
	roles[ArtistsRole] = "artists";
	roles[AlbumRole] = "album";
	roles[DurationRole] = "duration";
	roles[CoverUrlRole] = "coverUrl";
	roles[IsLoadedRole] = "isLoaded";
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
	map.insert(QStringLiteral("providerId"), s.providerId);
	map.insert(QStringLiteral("source"), s.source);
	map.insert(QStringLiteral("title"), s.name);
	QStringList names;
	for (const Artist &a : s.artists)
		names.append(a.name);
	map.insert(QStringLiteral("artists"), names.join(QStringLiteral(" / ")));
	map.insert(QStringLiteral("album"), s.album.name);
	map.insert(QStringLiteral("duration"), static_cast<qint64>(s.durationMs));
	map.insert(QStringLiteral("coverUrl"), s.album.coverUrl);
	return map;
}

void SongListModel::append(const Song &song)
{
    int pos = m_songs.size();
    beginInsertRows(QModelIndex(), pos, pos);
    m_songs.append(song);
    endInsertRows();
}

void SongListModel::append(const QList<Song> &songs)
{
    if (songs.isEmpty()) return;
    int pos = m_songs.size();
    beginInsertRows(QModelIndex(), pos, pos + songs.size() - 1);
    m_songs.append(songs);
    endInsertRows();
}

void SongListModel::insert(int index, const Song &song)
{
    int pos = qBound(0, index, m_songs.size());
    beginInsertRows(QModelIndex(), pos, pos);
    m_songs.insert(pos, song);
    endInsertRows();
}

void SongListModel::removeAt(int index)
{
    if (index < 0 || index >= m_songs.size())
        return;
    beginRemoveRows(QModelIndex(), index, index);
    m_songs.removeAt(index);
    endRemoveRows();
}

void SongListModel::clear()
{
    if (m_songs.isEmpty())
        return;
    beginResetModel();
    m_songs.clear();
    endResetModel();
}

void SongListModel::move(int from, int to)
{
    if (from < 0 || from >= m_songs.size() || to < 0 || to >= m_songs.size() || from == to)
        return;

    // Adjust destination index for beginMoveRows
    // If moving down (from < to), items shift up, so insertion point is to + 1
    int dest = (to > from) ? (to + 1) : to;
    
    if (beginMoveRows(QModelIndex(), from, from, QModelIndex(), dest)) {
        m_songs.move(from, to);
        endMoveRows();
        emit itemMoved(from, to);
    }
}

void SongListModel::setTotalCount(int count)
{
	beginResetModel();
	m_songs.clear();
	if (count > 0)
		m_songs.resize(count);
	endResetModel();
}

void SongListModel::updateRange(int start, const QList<Song> &songs)
{
	if (start < 0 || start + songs.size() > m_songs.size())
		return;
	for (int i = 0; i < songs.size(); ++i)
	{
		m_songs[start + i] = songs.at(i);
	}
	emit dataChanged(index(start), index(start + songs.size() - 1));
}

void SongListModel::clearRange(int start, int count)
{
    if (start < 0 || count <= 0 || start + count > m_songs.size())
        return;
    for(int i=0; i<count; ++i) {
        m_songs[start + i] = Song(); // Reset to empty
    }
    emit dataChanged(index(start), index(start + count - 1));
}

bool SongListModel::isLoaded(int row) const
{
	if (row < 0 || row >= m_songs.size())
		return false;
	return !m_songs.at(row).id.isEmpty();
}

}
