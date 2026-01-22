#pragma once

#include <QAbstractListModel>

#include "core_types.h"

namespace App
{

class LyricListModel : public QAbstractListModel
{
	Q_OBJECT

public:
	enum Roles
	{
		TimeMsRole = Qt::UserRole + 1,
		TextRole
	};

	explicit LyricListModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	void setLyric(const Lyric &lyric);
	const Lyric &lyric() const;

private:
	Lyric m_lyric;
};

}

