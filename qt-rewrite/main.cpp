// 应用入口：初始化 Qt 应用与日志系统，并加载 QML 主界面
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "core_types.h"
#include "logger.h"
#include "music_controller.h"

// 程序主函数
int main(int argc, char *argv[])
{
	// 设置应用组织与名称，影响设置存储位置等
	QCoreApplication::setOrganizationName("I-love-you");
	QCoreApplication::setApplicationName("QtRewrite");

	// 初始化日志系统，默认使用 Info 级别，支持 --debug 参数开启调试日志
	App::Logger::Level logLevel = App::Logger::Level::Info;
	for (int i = 1; i < argc; ++i) {
		if (QString::fromLocal8Bit(argv[i]) == "--debug") {
			logLevel = App::Logger::Level::Debug;
			break;
		}
	}
	App::Logger::init(logLevel);
	App::Logger::info("Application starting");

	// 创建 GUI 应用对象
	QQuickStyle::setStyle("Basic");
	QGuiApplication app(argc, argv);

	// 创建 QML 引擎并加载主 QML 模块
	QQmlApplicationEngine engine;
	auto *musicController = new App::MusicController(&engine);
	engine.rootContext()->setContextProperty("musicController", musicController);
	// 当根对象创建失败时，退出应用，避免进入不一致状态
	QObject::connect(
		&engine,
		&QQmlApplicationEngine::objectCreationFailed,
		&app,
		[]() { QCoreApplication::exit(-1); },
		Qt::QueuedConnection);
	engine.loadFromModule("qtrewrite", "Main");

	// 进入 Qt 事件循环
	return app.exec();
}
