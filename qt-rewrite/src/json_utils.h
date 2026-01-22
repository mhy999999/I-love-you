#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "core_types.h"

namespace App
{
namespace Json
{

Result<QString> readString(const QJsonObject &obj, const QString &key, bool required = true);
Result<qint64> readInt64(const QJsonObject &obj, const QString &key, bool required = true);
Result<int> readInt(const QJsonObject &obj, const QString &key, bool required = true);
Result<double> readDouble(const QJsonObject &obj, const QString &key, bool required = true);
Result<bool> readBool(const QJsonObject &obj, const QString &key, bool required = true);
Result<QJsonObject> readObject(const QJsonObject &obj, const QString &key, bool required = true);
Result<QJsonArray> readArray(const QJsonObject &obj, const QString &key, bool required = true);

}
}

