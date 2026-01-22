#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "core_types.h"
#include "logger.h"

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationName("I-love-you");
	QCoreApplication::setApplicationName("QtRewrite");
	App::Logger::init(App::Logger::Level::Debug);
	App::Logger::info("Application starting");

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("qtrewrite", "Main");

    return app.exec();
}
