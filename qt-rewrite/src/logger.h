// 简单日志封装，统一控制日志级别与输出
#pragma once

#include <QString>

namespace App
{

class Logger
{
public:
	// 日志级别，按严重程度递增
	enum class Level
	{
		Debug,
		Info,
		Warning,
		Error,
		None
	};

	// 初始化日志模块，设置初始日志级别
	static void init(Level level = Level::Info);
	// 动态调整日志级别
	static void setLevel(Level level);
	// 获取当前日志级别
	static Level level();

	// 输出调试日志
	static void debug(const QString &message);
	// 输出普通信息日志
	static void info(const QString &message);
	// 输出警告日志
	static void warning(const QString &message);
	// 输出错误日志
	static void error(const QString &message);

private:
	static Level currentLevel;
};

}
