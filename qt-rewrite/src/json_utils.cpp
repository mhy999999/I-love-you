// JsonUtils 实现：统一 JSON 字段读取与错误构造
#include "json_utils.h"

// 匿名命名空间内的辅助模板函数，仅在当前编译单元可见
namespace
{

// 构造“缺少字段”错误结果
template <typename T>
App::Result<T> missingField(const QString &key)
{
	App::Error e;
	e.category = App::ErrorCategory::Parser;
	e.code = 1;
	e.message = QStringLiteral("Missing field: ") + key;
	return App::Result<T>::failure(e);
}

// 构造“类型不匹配”错误结果
template <typename T>
App::Result<T> typeError(const QString &key, const QString &expected)
{
	App::Error e;
	e.category = App::ErrorCategory::Parser;
	e.code = 2;
	e.message = QStringLiteral("Invalid type for field: ") + key + QStringLiteral(", expected ") + expected;
	return App::Result<T>::failure(e);
}

}

namespace App
{
namespace Json
{

// 宽容读取字符串字段：接受 string/number/bool，并统一输出 QString
Result<QString> readString(const QJsonObject &obj, const QString &key, bool required)
{
	if (!obj.contains(key) || obj.value(key).isNull())
	{
		if (required)
			return missingField<QString>(key);
		return Result<QString>::success(QString());
	}
	QJsonValue v = obj.value(key);
	if (v.isString())
		return Result<QString>::success(v.toString());
	if (v.isDouble())
		return Result<QString>::success(QString::number(v.toDouble()));
	if (v.isBool())
		return Result<QString>::success(v.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
	return typeError<QString>(key, QStringLiteral("string"));
}

// 宽容读取 64 位整数：接受 number 或字符串数字
Result<qint64> readInt64(const QJsonObject &obj, const QString &key, bool required)
{
	if (!obj.contains(key) || obj.value(key).isNull())
	{
		if (required)
			return missingField<qint64>(key);
		return Result<qint64>::success(0);
	}
	QJsonValue v = obj.value(key);
	if (v.isDouble())
		return Result<qint64>::success(static_cast<qint64>(v.toDouble()));
	if (v.isString())
	{
		bool ok = false;
		qint64 value = v.toString().toLongLong(&ok);
		if (ok)
			return Result<qint64>::success(value);
	}
	return typeError<qint64>(key, QStringLiteral("integer"));
}

// 基于 readInt64 实现 32 位整数读取
Result<int> readInt(const QJsonObject &obj, const QString &key, bool required)
{
	Result<qint64> r = readInt64(obj, key, required);
	if (!r.ok)
		return Result<int>::failure(r.error);
	return Result<int>::success(static_cast<int>(r.value));
}

// 宽容读取浮点数：接受 number 或字符串数字
Result<double> readDouble(const QJsonObject &obj, const QString &key, bool required)
{
	if (!obj.contains(key) || obj.value(key).isNull())
	{
		if (required)
			return missingField<double>(key);
		return Result<double>::success(0.0);
	}
	QJsonValue v = obj.value(key);
	if (v.isDouble())
		return Result<double>::success(v.toDouble());
	if (v.isString())
	{
		bool ok = false;
		double value = v.toString().toDouble(&ok);
		if (ok)
			return Result<double>::success(value);
	}
	return typeError<double>(key, QStringLiteral("number"));
}

// 宽容读取布尔值：接受 bool/number/string 等多种表示
Result<bool> readBool(const QJsonObject &obj, const QString &key, bool required)
{
	if (!obj.contains(key) || obj.value(key).isNull())
	{
		if (required)
			return missingField<bool>(key);
		return Result<bool>::success(false);
	}
	QJsonValue v = obj.value(key);
	if (v.isBool())
		return Result<bool>::success(v.toBool());
	if (v.isDouble())
		return Result<bool>::success(v.toDouble() != 0.0);
	if (v.isString())
	{
		const QString s = v.toString().trimmed().toLower();
		if (s == QStringLiteral("true") || s == QStringLiteral("1"))
			return Result<bool>::success(true);
		if (s == QStringLiteral("false") || s == QStringLiteral("0"))
			return Result<bool>::success(false);
	}
	return typeError<bool>(key, QStringLiteral("bool"));
}

// 读取子对象字段
Result<QJsonObject> readObject(const QJsonObject &obj, const QString &key, bool required)
{
	if (!obj.contains(key) || obj.value(key).isNull())
	{
		if (required)
			return missingField<QJsonObject>(key);
		return Result<QJsonObject>::success(QJsonObject());
	}
	QJsonValue v = obj.value(key);
	if (v.isObject())
		return Result<QJsonObject>::success(v.toObject());
	return typeError<QJsonObject>(key, QStringLiteral("object"));
}

// 读取数组字段
Result<QJsonArray> readArray(const QJsonObject &obj, const QString &key, bool required)
{
	if (!obj.contains(key) || obj.value(key).isNull())
	{
		if (required)
			return missingField<QJsonArray>(key);
		return Result<QJsonArray>::success(QJsonArray());
	}
	QJsonValue v = obj.value(key);
	if (v.isArray())
		return Result<QJsonArray>::success(v.toArray());
	return typeError<QJsonArray>(key, QStringLiteral("array"));
}

}
}
