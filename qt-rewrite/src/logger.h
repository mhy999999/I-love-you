#pragma once

#include <QString>

namespace App
{

class Logger
{
public:
	enum class Level
	{
		Debug,
		Info,
		Warning,
		Error,
		None
	};

	static void init(Level level = Level::Info);
	static void setLevel(Level level);
	static Level level();

	static void debug(const QString &message);
	static void info(const QString &message);
	static void warning(const QString &message);
	static void error(const QString &message);

private:
	static Level currentLevel;
};

}

