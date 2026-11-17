#include "backend.h"
#include "theme.h"
#include "translator.h"
#include "tray.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
int main(int argc, char **argv) {
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    const bool capture = args.contains("--capture");
    const QString action = args.isEmpty() ? "open" : args.first();
    const bool cli =
        QStringList{"status", "start", "stop", "retry", "restore-wireless", "skill", "--help", "--version"}
            .contains(action);
    if (cli || capture)
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName("phonelink");
    app.setOrganizationName("omarchy");
    app.setApplicationVersion("0.0.2");
    app.setDesktopFileName("phonelink");
    app.setWindowIcon(QIcon(":/phonelink.svg"));
    app.setQuitOnLastWindowClosed(false);
    if (action == "--help") {
        QTextStream(stdout)
            << "phonelink open | status [--json] | start | stop | retry | restore-wireless | --tray\n";
        return 0;
    }
    if (action == "--version") {
        QTextStream(stdout) << "0.0.2\n";
        return 0;
    }
    if (action == "skill") {
        if (args.size() < 2 || args[1] != "install") {
            QTextStream(stderr) << "Use phonelink skill install --json\n";
            return 1;
        }
        const QString root =
            qEnvironmentVariable("CODEX_HOME", QDir::homePath() + "/.codex") + "/skills/phonelink";
        QFile resource(":/agent-skill.md");
        QSaveFile output(root + "/SKILL.md");
        if (!QDir().mkpath(root) || !resource.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
            return 1;
        const QByteArray guide = resource.readAll();
        if (output.write(guide) != guide.size() || !output.commit())
            return 1;
        QTextStream(stdout)
            << QJsonDocument(QJsonObject{{"installed", root + "/SKILL.md"}}).toJson(QJsonDocument::Compact)
            << '\n';
        return 0;
    }
    auto bus = QDBusConnection::sessionBus();
    const bool demo = args.contains("--demo");
    if (!demo && !bus.registerService("org.omarchy.Phonelink")) {
        const QString method = action == "status"             ? "Status"
                               : action == "start"            ? "Start"
                               : action == "stop"             ? "Stop"
                               : action == "retry"            ? "Retry"
                               : action == "restore-wireless" ? "RestoreWireless"
                                                              : "Show";
        QDBusInterface existing("org.omarchy.Phonelink", "/Phonelink", "org.omarchy.Phonelink", bus);
        auto reply = existing.call(method);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            QTextStream(stderr) << reply.errorMessage() << '\n';
            return 1;
        }
        if (action == "status" && !reply.arguments().isEmpty())
            QTextStream(stdout) << reply.arguments().first().toString() << '\n';
        return 0;
    }
    if (cli) {
        QTextStream(stderr)
            << "Phonelink service is not running. Start phonelink.service or run phonelink open.\n";
        return 1;
    }
    QQuickStyle::setStyle("Material");
    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Backend backend(nullptr, !demo);
    if (!demo)
        bus.registerObject("/Phonelink", &backend, QDBusConnection::ExportAllSlots);
    QObject trayOwner;
    Tray *tray = demo ? nullptr : new Tray(&trayOwner, &backend);
    Q_UNUSED(tray);
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("startHidden", action == "--tray" && !backend.preferences().value("showPanel").toBool());
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->setProperty("captureMode", capture);
    if (demo && args.contains("--expanded")) window->setProperty("navigationExpanded", true);
    const int tabIndex = args.indexOf("--tab");
    if (demo && tabIndex >= 0 && tabIndex + 1 < args.size())
        window->setProperty("selectedTab", args[tabIndex + 1].toInt());
    const int sizeIndex = args.indexOf("--size");
    if (demo && sizeIndex >= 0 && sizeIndex + 1 < args.size()) {
        const auto size = args[sizeIndex + 1].split('x');
        if (size.size() == 2)
            window->resize(size[0].toInt(), size[1].toInt());
    }
    QObject::connect(&backend, &Backend::showRequested, window, [window] {
        window->show();
        window->raise();
        window->requestActivate();
    });
    if (capture) {
        const int index = args.indexOf("--capture");
        if (index + 1 >= args.size())
            return 1;
        QTimer::singleShot(1500, &app, [&app, window, args, index] {
            app.exit(window->grabWindow().save(args[index + 1]) ? 0 : 1);
        });
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &backend, &Backend::Stop);
    return app.exec();
}
