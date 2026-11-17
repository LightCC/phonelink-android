#include "tray.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QDBusMetaType>
#include <QIcon>
#include <QImage>
#include <QDateTime>
#include <QFile>
#include <QStandardPaths>
Tray::Tray(QObject *owner, Backend *backend) : QDBusAbstractAdaptor(owner), m_backend(backend) {
    qDBusRegisterMetaType<TrayPixmap>();
    qDBusRegisterMetaType<TrayPixmaps>();
    m_service = QString("org.kde.StatusNotifierItem-%1-1").arg(QCoreApplication::applicationPid());
    auto bus = QDBusConnection::sessionBus();
    bus.registerService(m_service);
    bus.registerObject("/StatusNotifierItem", owner, QDBusConnection::ExportAdaptors);
    m_watcher.setConnection(bus);
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForRegistration);
    m_watcher.addWatchedService("org.kde.StatusNotifierWatcher");
    connect(&m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] { registerTray(); });
    connect(backend, &Backend::changed, this, [this] {
        const auto icon = iconKey();
        if (icon != m_lastIcon) {
            m_lastIcon = icon;
            emit NewIcon();
        }
    });
    m_pluginPoll.setInterval(1000);
    connect(&m_pluginPoll, &QTimer::timeout, this, [this] {
        const auto current = status();
        if (current != m_lastStatus) { m_lastStatus = current; emit NewStatus(current); }
    });
    m_pluginPoll.start();
    registerTray();
}
QString Tray::iconKey() const {
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

// Send embedded icon pixels so user installs and fresh tray hosts do not depend on icon-theme caches.
TrayPixmaps Tray::iconPixmap() const {
    TrayPixmaps pixmaps;
    for (int size : {24, 32, 64}) {
        const QImage image = QIcon(":/" + iconKey() + ".svg").pixmap(size, size).toImage().convertToFormat(QImage::Format_ARGB32);
        if (image.isNull()) continue;
        QByteArray bytes;
        bytes.reserve(image.width() * image.height() * 4);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = image.pixel(x, y);
                // The StatusNotifierItem wire format is network-order ARGB, independent of CPU byte order.
                bytes.append(char(qAlpha(pixel))); bytes.append(char(qRed(pixel)));
                bytes.append(char(qGreen(pixel))); bytes.append(char(qBlue(pixel)));
            }
        }
        pixmaps.append({image.width(), image.height(), bytes});
    }
    return pixmaps;
}
QDBusArgument &operator<<(QDBusArgument &argument, const TrayPixmap &pixmap) {
    argument.beginStructure(); argument << pixmap.width << pixmap.height << pixmap.bytes; argument.endStructure(); return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, TrayPixmap &pixmap) {
    argument.beginStructure(); argument >> pixmap.width >> pixmap.height >> pixmap.bytes; argument.endStructure(); return argument;
}

// A short lease restores standalone tray access even if the shell crashes or the widget is removed.
bool Tray::pluginLeaseActive(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = file.read(65);
    if (bytes.size() > 64) return false;
    bool ok = false;
    const qint64 expiry = bytes.trimmed().toLongLong(&ok);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return ok && expiry > now && expiry <= now + 30;
}
QString Tray::status() const {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/phonelink-plugin-active";
    return pluginLeaseActive(path) ? "Passive" : "Active";
}
