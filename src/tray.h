#pragma once
#include "backend.h"
#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QList>

struct TrayPixmap { int width; int height; QByteArray bytes; };
Q_DECLARE_METATYPE(TrayPixmap)
using TrayPixmaps = QList<TrayPixmap>;
Q_DECLARE_METATYPE(TrayPixmaps)
QDBusArgument &operator<<(QDBusArgument &argument, const TrayPixmap &pixmap);
const QDBusArgument &operator>>(const QDBusArgument &argument, TrayPixmap &pixmap);
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
class Tray : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category READ category)
    Q_PROPERTY(QString Id READ id)
    Q_PROPERTY(QString Title READ title)
    Q_PROPERTY(QString Status READ status)
    Q_PROPERTY(QString IconName READ iconName)
    Q_PROPERTY(TrayPixmaps IconPixmap READ iconPixmap)
    Q_PROPERTY(QString IconThemePath READ iconThemePath)
    Q_PROPERTY(QString AttentionIconName READ iconName)
    Q_PROPERTY(QString OverlayIconName READ overlay)
    Q_PROPERTY(uint WindowId READ windowId)
    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu)
    Q_PROPERTY(QDBusObjectPath Menu READ menu)
  public:
    Tray(QObject *owner, Backend *backend);
    QString category() const { return "SystemServices"; }
    QString id() const { return "phonelink"; }
    QString title() const { return "Phonelink"; }
    QString status() const { return "Active"; }
    QString iconName() const { return {}; }
    TrayPixmaps iconPixmap() const;
    QString iconThemePath() const { return {}; }
    QString overlay() const { return {}; }
    uint windowId() const { return 0; }
    bool itemIsMenu() const { return false; }
    QDBusObjectPath menu() const { return QDBusObjectPath("/"); }
  public slots:
    void Activate(int, int) { m_backend->Show(); }
    void SecondaryActivate(int, int) { m_backend->Show(); }
    void ContextMenu(int, int) { m_backend->Show(); }
    void Scroll(int, const QString &) {}
  signals:
    void NewIcon();
    void NewTitle();
    void NewStatus(const QString &status);

  private:
    void registerTray();
    QString iconKey() const;
    Backend *m_backend;
    QDBusServiceWatcher m_watcher;
    QString m_service, m_lastIcon;
};
