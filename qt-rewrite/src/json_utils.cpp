#include "json_utils.h"

namespace
{

template <typename T>
App::Result<T> missingField(const QString &key)
{
	App::Error e;
	e.category = App::ErrorCategory::Parser;
	e.code = 1;
	e.message = QStringLiteral("Missing field: ") + key;
	return App::Result<T>::failure(e);
}

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

Result<int> readInt(const QJsonObject &obj, const QString &key, bool required)
{
	Result<qint64> r = readInt64(obj, key, required);
	if (!r.ok)
		return Result<int>::failure(r.error);
	return Result<int>::success(static_cast<int>(r.value));
}

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

