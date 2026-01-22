// JSON 解析辅助函数，提供宽容的字段读取与统一错误处理
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "core_types.h"

namespace App
{
namespace Json
{

// 读取字符串字段，可容忍数字 / 布尔并转换为字符串
Result<QString> readString(const QJsonObject &obj, const QString &key, bool required = true);
// 读取 64 位整数，支持字符串数字转换
Result<qint64> readInt64(const QJsonObject &obj, const QString &key, bool required = true);
// 读取 32 位整数，基于 readInt64 实现
Result<int> readInt(const QJsonObject &obj, const QString &key, bool required = true);
// 读取浮点数，支持字符串数字转换
Result<double> readDouble(const QJsonObject &obj, const QString &key, bool required = true);
// 读取布尔值，支持 0/1 与 "true"/"false" 等表示
Result<bool> readBool(const QJsonObject &obj, const QString &key, bool required = true);
// 读取对象字段
Result<QJsonObject> readObject(const QJsonObject &obj, const QString &key, bool required = true);
// 读取数组字段
Result<QJsonArray> readArray(const QJsonObject &obj, const QString &key, bool required = true);

}
}
