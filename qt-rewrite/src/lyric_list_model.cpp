#include "lyric_list_model.h"

namespace App
{

LyricListModel::LyricListModel(QObject *parent)
	: QAbstractListModel(parent)
{
}

int LyricListModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return m_lyric.lines.size();
}

QVariant LyricListModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_lyric.lines.size())
		return {};
	const LyricLine &line = m_lyric.lines.at(index.row());
	switch (role)
	{
	case TimeMsRole:
		return static_cast<qint64>(line.timeMs);
	case TextRole:
		return line.text;
	default:
		return {};
	}
}

QHash<int, QByteArray> LyricListModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[TimeMsRole] = "timeMs";
	roles[TextRole] = "text";
	return roles;
}

void LyricListModel::setLyric(const Lyric &lyric)
{
	beginResetModel();
	m_lyric = lyric;
	endResetModel();
}

const Lyric &LyricListModel::lyric() const
{
	return m_lyric;
}

}

