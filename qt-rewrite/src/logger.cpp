// Logger 实现：基于 Qt 的 qDebug 系列函数封装日志输出
#include "logger.h"

#include <QDebug>

namespace App
{

// 默认日志级别为 Info
Logger::Level Logger::currentLevel = Logger::Level::Info;

// 初始化当前日志级别
void Logger::init(Level level)
{
	currentLevel = level;
}

// 运行时调整日志级别
void Logger::setLevel(Level level)
{
	currentLevel = level;
}

// 获取当前日志级别
Logger::Level Logger::level()
{
	return currentLevel;
}

// 输出调试日志（仅 Debug 级别及以上可见）
void Logger::debug(const QString &message)
{
	if (currentLevel <= Level::Debug)
		qDebug().noquote() << message;
}

// 输出普通信息日志
void Logger::info(const QString &message)
{
	if (currentLevel <= Level::Info)
		qInfo().noquote() << message;
}

// 输出警告日志
void Logger::warning(const QString &message)
{
	if (currentLevel <= Level::Warning)
		qWarning().noquote() << message;
}

// 输出错误日志
void Logger::error(const QString &message)
{
	if (currentLevel <= Level::Error)
		qCritical().noquote() << message;
}

}
