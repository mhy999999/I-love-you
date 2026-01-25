#include "playlist_list_model.h"

namespace App
{

PlaylistListModel::PlaylistListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PlaylistListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_playlists.size();
}

QVariant PlaylistListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_playlists.size())
        return QVariant();

    const auto &playlist = m_playlists[index.row()];

    switch (role)
    {
    case IdRole:
        return playlist.id;
    case NameRole:
        return playlist.name;
    case CoverUrlRole:
        return playlist.coverUrl;
    case DescriptionRole:
        return playlist.description;
    case TrackCountRole:
        return playlist.trackCount;
    }

    return QVariant();
}

QHash<int, QByteArray> PlaylistListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[CoverUrlRole] = "coverUrl";
    roles[DescriptionRole] = "description";
    roles[TrackCountRole] = "trackCount";
    return roles;
}

void PlaylistListModel::setPlaylists(const QList<PlaylistMeta> &playlists)
{
    beginResetModel();
    m_playlists = playlists;
    endResetModel();
}

const QList<PlaylistMeta> &PlaylistListModel::playlists() const
{
    return m_playlists;
}

QVariantMap PlaylistListModel::get(int row) const
{
    if (row < 0 || row >= m_playlists.size())
        return QVariantMap();

    const auto &playlist = m_playlists[row];
    return {
        {"id", playlist.id},
        {"name", playlist.name},
        {"coverUrl", playlist.coverUrl},
        {"description", playlist.description},
        {"trackCount", playlist.trackCount}
    };
}

void PlaylistListModel::append(const PlaylistMeta &playlist)
{
    beginInsertRows(QModelIndex(), m_playlists.size(), m_playlists.size());
    m_playlists.append(playlist);
    endInsertRows();
}

void PlaylistListModel::clear()
{
    beginResetModel();
    m_playlists.clear();
    endResetModel();
}

void PlaylistListModel::updateTrackCount(const QString &playlistId, int delta)
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        if (m_playlists[i].id == playlistId) {
            m_playlists[i].trackCount += delta;
            // Ensure non-negative
            if (m_playlists[i].trackCount < 0) m_playlists[i].trackCount = 0;
            
            emit dataChanged(index(i), index(i), {TrackCountRole});
            break;
        }
    }
}

}
