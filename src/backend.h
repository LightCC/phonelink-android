#pragma once
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>
#include <functional>

class Backend : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.omarchy.Phonelink")
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY changed)
    Q_PROPERTY(QString recentLog READ recentLog NOTIFY changed)
    Q_PROPERTY(QString historyDirectory READ historyDirectory CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
  public:
    explicit Backend(QObject *parent = nullptr, bool monitor = true);
    ~Backend() override;
    QVariantMap state() const;
    QVariantMap preferences() const;
    QString recentLog() const { return m_recent.join('\n'); }
    QString historyDirectory() const { return m_history; }
    bool busy() const { return m_busy || m_bootstrapping; }
    static bool validEndpoint(const QString &endpoint);
    static int retryDelay(int attempt);
    static QMap<QString, QString> parseDevices(const QString &output);
    Q_INVOKABLE bool savePreferences(const QString &serial, const QString &endpoint, const QString &transport,
                                     bool retry, bool audio, bool bootstrap, bool autoOpen, bool showPanel = false);
    Q_INVOKABLE void openHistory();
    Q_INVOKABLE void refresh();
  public slots:
    void Show() { emit showRequested(); }
    void Start();
    void Stop();
    void Retry();
    void RestoreWireless();
    QString Status() const;
  signals:
    void changed();
    void showRequested();

  private:
    using Done = std::function<void(int, QString)>;
    void command(const QString &program, const QStringList &args, Done done, int timeout = 4000);
    void inspectWireless(const QMap<QString, QString> &devices, const QString &advertised);
    void verifyWireless(const QString &target, const QString &endpoint);
    void finishScan();
    void bootstrap();
    void maybeStart();
    void mirrorEnded(int code, QProcess::ExitStatus status);
    void record(const QString &event, const QString &detail = {});
    void rememberPhone(const QString &target, const QString &output, const QString &endpoint);
    void updateUsbPresence();
    QString adb() const;
    QString scrcpy() const;
    QSettings m_settings;
    QTimer m_poll, m_retryTimer, m_stableTimer;
    QProcess m_mirror;
    QString m_history, m_usbStatus = "absent", m_wirelessStatus = "not discovered", m_endpoint,
                       m_wirelessTarget;
    QString m_mirrorStatus = "Stopped", m_error, m_usbId, m_activeTarget, m_model, m_android;
    QStringList m_recent;
    QJsonObject m_profiles;
    QByteArray m_lastSnapshot;
    bool m_busy = false, m_bootstrapping = false, m_bootstrapRequested = false, m_bootstrapAttempted = false;
    bool m_usbPresent = false, m_usbReady = false, m_wirelessReady = false, m_wanted = false,
         m_shuttingDown = false;
    bool m_deliberateStop = false;
    int m_attempt = 0;
    qint64 m_lastConnect = 0;
};
