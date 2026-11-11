#include "tray.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
Tray::Tray(QObject *owner, Backend *backend) : QDBusAbstractAdaptor(owner), m_backend(backend) {
    m_service = QString("org.kde.StatusNotifierItem-%1-1").arg(QCoreApplication::applicationPid());
    auto bus = QDBusConnection::sessionBus();
    bus.registerService(m_service);
    bus.registerObject("/StatusNotifierItem", owner, QDBusConnection::ExportAdaptors);
    m_watcher.setConnection(bus);
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForRegistration);
    m_watcher.addWatchedService("org.kde.StatusNotifierWatcher");
    connect(&m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] { registerTray(); });
    connect(backend, &Backend::changed, this, [this] {
        const auto icon = iconName();
        if (icon != m_lastIcon) {
            m_lastIcon = icon;
            emit NewIcon();
        }
    });
    registerTray();
}
QString Tray::iconName() const {
    const auto s = m_backend->state();
    if (s.value("running").toBool())
        return "phonelink-active";
    if (s.value("usbReady").toBool() || s.value("wirelessReady").toBool())
        return "phonelink";
    return "phonelink-offline";
}
void Tray::registerTray() {
    QDBusInterface watcher("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                           "org.kde.StatusNotifierWatcher", QDBusConnection::sessionBus());
    auto *call =
        new QDBusPendingCallWatcher(watcher.asyncCall("RegisterStatusNotifierItem", m_service), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [call] {
        QDBusPendingReply<> reply = *call;
        if (reply.isError())
            qWarning() << "Tray registration:" << reply.error().message();
        call->deleteLater();
    });
}
