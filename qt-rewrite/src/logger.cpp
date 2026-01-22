#include "logger.h"

#include <QDebug>

namespace App
{

Logger::Level Logger::currentLevel = Logger::Level::Info;

void Logger::init(Level level)
{
	currentLevel = level;
}

void Logger::setLevel(Level level)
{
	currentLevel = level;
}

Logger::Level Logger::level()
{
	return currentLevel;
}

void Logger::debug(const QString &message)
{
	if (currentLevel <= Level::Debug)
		qDebug().noquote() << message;
}

void Logger::info(const QString &message)
{
	if (currentLevel <= Level::Info)
		qInfo().noquote() << message;
}

void Logger::warning(const QString &message)
{
	if (currentLevel <= Level::Warning)
		qWarning().noquote() << message;
}

void Logger::error(const QString &message)
{
	if (currentLevel <= Level::Error)
		qCritical().noquote() << message;
}

}

