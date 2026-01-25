#pragma once

#include <QAbstractListModel>
#include "core_types.h"

namespace App
{

class PlaylistListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CoverUrlRole,
        DescriptionRole,
        TrackCountRole,
        CreatorIdRole
    };

    explicit PlaylistListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPlaylists(const QList<PlaylistMeta> &playlists);
    const QList<PlaylistMeta> &playlists() const;
    Q_INVOKABLE QVariantMap get(int row) const;

    void append(const PlaylistMeta &playlist);
    void clear();

    void updateTrackCount(const QString &playlistId, int delta);

private:
    QList<PlaylistMeta> m_playlists;
};

}
