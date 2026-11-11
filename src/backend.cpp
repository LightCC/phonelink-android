#include "backend.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <memory>

Backend::Backend(QObject *parent, bool monitor) : QObject(parent), m_settings("omarchy", "phonelink") {
    m_history = QStandardPaths::writableLocation(QStandardPaths::StateLocation) + "/history";
    QDir().mkpath(m_history);
    QFile::setPermissions(QFileInfo(m_history).absolutePath(),
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QFile::setPermissions(m_history, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QFile profiles(m_history + "/phones.json");
    if (profiles.open(QIODevice::ReadOnly))
        m_profiles = QJsonDocument::fromJson(profiles.readAll()).object();
    m_poll.setInterval(5000);
    connect(&m_poll, &QTimer::timeout, this, &Backend::refresh);
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, [this] { refresh(); });
    m_stableTimer.setSingleShot(true);
    m_stableTimer.setInterval(60000);
    connect(&m_stableTimer, &QTimer::timeout, this, [this] { m_attempt = 0; });
    connect(&m_mirror, &QProcess::finished, this, &Backend::mirrorEnded);
    connect(&m_mirror, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            mirrorEnded(-1, QProcess::CrashExit);
    });
    m_mirror.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_mirror, &QProcess::readyReadStandardOutput, this, [this] {
        const QString output = QString::fromUtf8(m_mirror.readAllStandardOutput()).trimmed();
        if (!output.isEmpty())
            record("scrcpy-output", output.left(8192));
    });
    if (monitor) {
        record("service-started");
        m_wanted = m_settings.value("autoOpen", false).toBool();
        m_poll.start();
        QTimer::singleShot(0, this, &Backend::refresh);
    }
}
Backend::~Backend() {
    m_shuttingDown = true;
    if (m_mirror.state() != QProcess::NotRunning) {
        m_mirror.terminate();
        if (!m_mirror.waitForFinished(1500)) {
            m_mirror.kill();
            m_mirror.waitForFinished(500);
        }
    }
    const auto processes = findChildren<QProcess *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *p : processes)
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(500);
        }
}
QString Backend::adb() const { return qEnvironmentVariable("PHONELINK_ADB", "/usr/bin/adb"); }
QString Backend::scrcpy() const { return qEnvironmentVariable("PHONELINK_SCRCPY", "/usr/bin/scrcpy"); }
QVariantMap Backend::preferences() const {
    return {{"serial", m_settings.value("serial").toString()},
            {"endpoint", m_settings.value("endpoint").toString()},
            {"transport", m_settings.value("transport", "auto").toString()},
            {"retry", m_settings.value("retry", true).toBool()},
            {"audio", m_settings.value("audio", false).toBool()},
            {"bootstrap", m_settings.value("bootstrap", true).toBool()},
            {"autoOpen", m_settings.value("autoOpen", false).toBool()},
            {"showPanel", m_settings.value("showPanel", false).toBool()}};
}
QVariantMap Backend::state() const {
    return {{"usbPresent", m_usbPresent},
            {"usb", m_usbStatus},
            {"usbReady", m_usbReady},
            {"usbId", m_usbId},
            {"wireless", m_wirelessStatus},
            {"wirelessReady", m_wirelessReady},
            {"endpoint", m_endpoint},
            {"mirror", m_mirrorStatus},
            {"target", m_activeTarget},
            {"wanted", m_wanted},
            {"running", m_mirror.state() != QProcess::NotRunning},
            {"error", m_error},
            {"model", m_model},
            {"android", m_android},
            {"attempt", m_attempt},
            {"serial", m_settings.value("serial").toString()},
            {"profiles", m_profiles.toVariantMap()}};
}
QString Backend::Status() const {
    return QString::fromUtf8(QJsonDocument::fromVariant(state()).toJson(QJsonDocument::Compact));
}
bool Backend::validEndpoint(const QString &endpoint) {
    const auto parts = endpoint.split(':');
    if (parts.size() != 2)
        return false;
    bool ok = false;
    const int port = parts[1].toInt(&ok);
    QHostAddress host(parts[0]);
    bool v4 = false;
    const quint32 ip = host.toIPv4Address(&v4);
    const bool local = (ip >> 24) == 10 || (ip >> 16) == 0xc0a8 || (ip >> 20) == 0xac1 || (ip >> 22) == 0x191;
    return ok && port > 0 && port < 65536 && v4 && local;
}
int Backend::retryDelay(int attempt) { return qMin(30000, 1000 * (1 << qBound(0, attempt - 1, 5))); }
QMap<QString, QString> Backend::parseDevices(const QString &output) {
    QMap<QString, QString> devices;
    for (const auto &line : output.split('\n')) {
        const auto fields = line.simplified().split(' ');
        if (fields.size() >= 2 && QStringList{"device", "offline", "unauthorized", "no"}.contains(fields[1]))
            devices[fields[0]] = fields[1];
    }
    return devices;
}
bool Backend::savePreferences(const QString &serial, const QString &endpoint, const QString &transport,
                              bool retry, bool audio, bool boot, bool autoOpen, bool showPanel) {
    static const QRegularExpression validSerial("^[A-Za-z0-9._-]{1,128}$");
    if (!validSerial.match(serial).hasMatch() || (!endpoint.isEmpty() && !validEndpoint(endpoint)) ||
        !QStringList{"auto", "usb", "wireless"}.contains(transport)) {
        m_error = tr("Enter a phone serial and a private IPv4 address:port, or leave the endpoint blank.");
        emit changed();
        return false;
    }
    if (busy() || m_mirror.state() != QProcess::NotRunning) {
        m_error = tr("Stop mirroring and wait for the current check before saving settings.");
        emit changed();
        return false;
    }
    m_settings.setValue("serial", serial);
    m_settings.setValue("endpoint", endpoint);
    m_settings.setValue("transport", transport);
    m_settings.setValue("retry", retry);
    m_settings.setValue("audio", audio);
    m_settings.setValue("bootstrap", boot);
    m_settings.setValue("autoOpen", autoOpen);
    m_settings.setValue("showPanel", showPanel);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        m_error = tr("Could not save settings.");
        emit changed();
        return false;
    }
    m_wirelessReady = false;
    m_wirelessTarget.clear();
    m_endpoint.clear();
    m_model.clear();
    m_android.clear();
    m_usbReady = false;
    m_error.clear();
    m_bootstrapAttempted = false;
    m_lastConnect = 0;
    record("settings-saved");
    refresh();
    return true;
}
void Backend::openHistory() {
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_history))) {
        m_error = tr("Could not open the history folder.");
        emit changed();
    }
}
void Backend::command(const QString &program, const QStringList &args, Done done, int timeout) {
    auto *p = new QProcess(this);
    auto *timer = new QTimer(p);
    timer->setSingleShot(true);
    auto finished = std::make_shared<bool>(false);
    auto timedOut = std::make_shared<bool>(false);
    auto output = std::make_shared<QByteArray>();
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, &QProcess::readyReadStandardOutput, p, [p, output] {
        output->append(p->readAllStandardOutput());
        if (output->size() > 65536)
            output->remove(0, output->size() - 65536);
    });
    auto complete = [this, p, timer, done, finished, timedOut, output](int code) {
        if (*finished)
            return;
        *finished = true;
        timer->stop();
        output->append(p->readAllStandardOutput());
        const QString text = QString::fromUtf8(output->right(65536));
        p->deleteLater();
        if (!m_shuttingDown)
            done(*timedOut ? -1 : code, text);
    };
    connect(timer, &QTimer::timeout, p, [p, timedOut] {
        *timedOut = true;
        p->kill();
    });
    connect(p, &QProcess::finished, p, [complete](int code, QProcess::ExitStatus s) {
        complete(s == QProcess::NormalExit ? code : -1);
    });
    connect(p, &QProcess::errorOccurred, p, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(-1);
    });
    p->start(program, args);
    timer->start(timeout);
}
void Backend::updateUsbPresence() {
    m_usbPresent = false;
    m_usbId.clear();
    const auto serial = m_settings.value("serial").toString();
    for (const auto &dir : QDir("/sys/bus/usb/devices").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        auto read = [&](const QString &name) {
            QFile f("/sys/bus/usb/devices/" + dir + "/" + name);
            return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
        };
        if (!serial.isEmpty() && read("serial") == serial) {
            m_usbPresent = true;
            m_usbId = read("idVendor") + ":" + read("idProduct");
            break;
        }
    }
}
void Backend::refresh() {
    if (m_busy || m_bootstrapping)
        return;
    m_busy = true;
    emit changed();
    updateUsbPresence();
    const QString serial = m_settings.value("serial").toString();
    if (serial.isEmpty()) {
        m_error = tr("Set the phone serial in Settings to begin.");
        finishScan();
        return;
    }
    if (!QFileInfo::exists(adb())) {
        m_error = tr("ADB is missing. Install android-tools.");
        finishScan();
        return;
    }
    command(adb(), {"devices", "-l"}, [this, serial](int code, QString out) {
        if (code != 0) {
            m_usbReady = false;
            m_wirelessReady = false;
            m_usbStatus = "check failed";
            m_wirelessStatus = "check failed";
            m_error = tr("ADB check failed: %1").arg(out.trimmed());
            finishScan();
            return;
        }
        const auto devices = parseDevices(out);
        m_usbReady = devices.value(serial) == "device";
        m_usbStatus = m_usbReady                 ? "authorized"
                      : devices.contains(serial) ? devices.value(serial)
                      : m_usbPresent             ? "USB debugging unavailable"
                                                 : "absent";
        if (!m_usbReady)
            m_bootstrapAttempted = false;
        command(adb(), {"mdns", "services"}, [this, devices, serial](int, QString mdns) {
            QString advertised;
            for (const auto &line : mdns.split('\n')) {
                const auto f = line.simplified().split(' ');
                if (f.size() >= 3 && f[0].startsWith("adb-" + serial + "-") &&
                    f[1].startsWith("_adb-tls-connect._tcp") && validEndpoint(f[2])) {
                    advertised = f[2];
                    break;
                }
            }
            inspectWireless(devices, advertised);
        });
    });
}
void Backend::inspectWireless(const QMap<QString, QString> &devices, const QString &advertised) {
    const QString serial = m_settings.value("serial").toString();
    const QString endpoint = advertised.isEmpty() ? m_settings.value("endpoint").toString() : advertised;
    QString target;
    if (devices.value(endpoint) == "device")
        target = endpoint;
    if (target.isEmpty())
        for (auto it = devices.begin(); it != devices.end(); ++it)
            if (it.key().startsWith("adb-" + serial + "-") && it.value() == "device") {
                target = it.key();
                break;
            }
    if (!target.isEmpty()) {
        verifyWireless(target, endpoint);
        return;
    }
    m_wirelessReady = false;
    m_wirelessTarget.clear();
    m_endpoint = endpoint;
    if (endpoint.isEmpty()) {
        m_wirelessStatus = "not discovered";
        finishScan();
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastConnect < 20000) {
        finishScan();
        return;
    }
    m_lastConnect = now;
    m_wirelessStatus = "connecting";
    emit changed();
    command(adb(), {"connect", endpoint}, [this, endpoint](int code, QString out) {
        if (code == 0 && (out.contains("connected to") || out.contains("already connected"))) {
            verifyWireless(endpoint, endpoint);
            return;
        }
        m_wirelessStatus = "unavailable";
        m_error = tr("Wireless connection failed: %1").arg(out.trimmed());
        finishScan();
    });
}
void Backend::verifyWireless(const QString &target, const QString &endpoint) {
    command(adb(),
            {"-s", target, "shell",
             "getprop ro.serialno; getprop ro.product.manufacturer; getprop ro.product.model; getprop "
             "ro.build.version.release"},
            [this, target, endpoint](int code, QString out) {
                const auto actual = out.section('\n', 0, 0).trimmed();
                if (code == 0 && actual == m_settings.value("serial").toString()) {
                    m_wirelessReady = true;
                    m_wirelessTarget = target;
                    m_endpoint = endpoint;
                    m_wirelessStatus = "authorized";
                    m_error.clear();
                    if (validEndpoint(endpoint))
                        m_settings.setValue("endpoint", endpoint);
                    rememberPhone(target, out, endpoint);
                } else {
                    m_wirelessReady = false;
                    m_wirelessTarget.clear();
                    m_wirelessStatus = actual.isEmpty() ? "unavailable" : "identity not verified";
                    m_error = tr("Wireless phone identity could not be verified.");
                }
                finishScan();
            });
}
void Backend::finishScan() {
    auto finish = [this] {
        m_busy = false;
        const auto snapshot = QJsonDocument::fromVariant(QVariantMap{{"usb", m_usbStatus},
                                                                     {"usbId", m_usbId},
                                                                     {"wireless", m_wirelessStatus},
                                                                     {"endpoint", m_endpoint},
                                                                     {"serial", m_settings.value("serial")}})
                                  .toJson(QJsonDocument::Compact);
        if (snapshot != m_lastSnapshot) {
            m_lastSnapshot = snapshot;
            record("connection-changed");
        }
        emit changed();
        if (m_bootstrapRequested ||
            (m_wanted && m_settings.value("transport", "auto").toString() != "usb" && !m_wirelessReady &&
             m_usbReady && !m_bootstrapAttempted && m_settings.value("bootstrap", true).toBool())) {
            bootstrap();
            return;
        }
        maybeStart();
    };
    if (m_usbReady) {
        const QString serial = m_settings.value("serial").toString();
        command(adb(),
                {"-s", serial, "shell",
                 "getprop ro.serialno; getprop ro.product.manufacturer; getprop ro.product.model; getprop "
                 "ro.build.version.release"},
                [this, serial, finish](int code, QString out) {
                    if (code == 0)
                        rememberPhone(serial, out, {});
                    finish();
                });
    } else
        finish();
}
void Backend::rememberPhone(const QString &target, const QString &output, const QString &endpoint) {
    const auto lines = output.split('\n');
    if (lines.size() < 4 || lines[0].trimmed() != m_settings.value("serial").toString())
        return;
    m_model = lines[2].trimmed();
    m_android = lines[3].trimmed();
    const QString serial = lines[0].trimmed(), now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QJsonObject phone = m_profiles.value(serial).toObject();
    const QJsonObject previous = phone;
    const bool metadataChanged = phone.value("model").toString() != m_model ||
                                 phone.value("android").toString() != m_android ||
                                 phone.value("manufacturer").toString() != lines[1].trimmed() ||
                                 (!m_usbId.isEmpty() && phone.value("usbId").toString() != m_usbId);
    if (phone.isEmpty())
        phone["firstSeen"] = now;
    phone["lastSeen"] = now;
    phone["serial"] = serial;
    phone["manufacturer"] = lines[1].trimmed();
    phone["model"] = m_model;
    phone["android"] = m_android;
    if (!m_usbId.isEmpty())
        phone["usbId"] = m_usbId;
    QJsonObject connections = phone.value("connections").toObject();
    const QString key = target == serial ? "usb:" + serial : validEndpoint(endpoint) ? endpoint : target;
    QJsonObject connection = connections.value(key).toObject();
    bool fresh = connection.isEmpty();
    if (fresh)
        connection["firstSeen"] = now;
    connection["lastSeen"] = now;
    connection["transport"] = target == serial ? "usb" : "wireless";
    connection["adbTarget"] = target;
    if (validEndpoint(endpoint)) {
        connection["endpoint"] = endpoint;
        connection["ip"] = endpoint.section(':', 0, 0);
        connection["port"] = endpoint.section(':', 1).toInt();
    }
    connections[key] = connection;
    phone["connections"] = connections;
    m_profiles[serial] = phone;
    QSaveFile file(m_history + "/phones.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        file.write(QJsonDocument(m_profiles).toJson());
        if (!file.commit())
            m_error = tr("Could not save phone history.");
    } else
        m_error = tr("Could not save phone history.");
    if (metadataChanged)
        record("phone-metadata-changed",
               QString::fromUtf8(
                   QJsonDocument(QJsonObject{{"previous", previous}}).toJson(QJsonDocument::Compact)));
    if (fresh)
        record("connection-remembered", key);
}
void Backend::RestoreWireless() {
    m_bootstrapRequested = true;
    m_bootstrapAttempted = false;
    if (!busy())
        bootstrap();
}
void Backend::bootstrap() {
    if (m_bootstrapping)
        return;
    m_bootstrapRequested = false;
    if (!m_usbReady) {
        m_error = tr("Connect USB and approve USB debugging on the phone first.");
        emit changed();
        return;
    }
    if (m_mirror.state() != QProcess::NotRunning && m_activeTarget != m_settings.value("serial").toString()) {
        m_error = tr("Stop the wireless screen before restoring wireless debugging.");
        emit changed();
        return;
    }
    m_bootstrapping = true;
    m_bootstrapAttempted = true;
    record("wireless-restore-started");
    emit changed();
    const QString serial = m_settings.value("serial").toString();
    command(adb(), {"-s", serial, "shell", "settings put global adb_wifi_enabled 0"},
            [this, serial](int code, QString out) {
                if (code != 0) {
                    m_bootstrapping = false;
                    m_error = tr("Could not reset wireless debugging: %1").arg(out.trimmed());
                    record("wireless-restore-failed", m_error);
                    emit changed();
                    return;
                }
                QTimer::singleShot(500, this, [this, serial] {
                    command(adb(), {"-s", serial, "shell", "settings put global adb_wifi_enabled 1"},
                            [this](int result, QString output) {
                                m_bootstrapping = false;
                                m_lastConnect = 0;
                                m_wirelessReady = false;
                                if (result != 0)
                                    m_error =
                                        tr("Could not enable wireless debugging: %1").arg(output.trimmed());
                                record(result == 0 ? "wireless-restore-requested" : "wireless-restore-failed",
                                       output.trimmed());
                                emit changed();
                                QTimer::singleShot(1200, this, &Backend::refresh);
                            });
                });
            });
}
void Backend::Start() {
    m_wanted = true;
    m_attempt = 0;
    m_error.clear();
    m_retryTimer.stop();
    m_bootstrapAttempted = false;
    m_lastConnect = 0;
    record("screen-requested");
    refresh();
}
void Backend::Stop() {
    m_wanted = false;
    m_retryTimer.stop();
    m_stableTimer.stop();
    m_deliberateStop = true;
    if (m_mirror.state() != QProcess::NotRunning) {
        m_mirror.terminate();
        QTimer::singleShot(1500, &m_mirror, [this] {
            if (m_deliberateStop && m_mirror.state() != QProcess::NotRunning)
                m_mirror.kill();
        });
    } else {
        m_mirrorStatus = "Stopped";
        record("screen-stopped");
    }
    emit changed();
}
void Backend::Retry() {
    m_attempt = 0;
    m_retryTimer.stop();
    m_error.clear();
    m_lastConnect = 0;
    m_bootstrapAttempted = false;
    record("manual-retry");
    refresh();
}
void Backend::maybeStart() {
    if (!m_wanted || m_retryTimer.isActive() || m_mirror.state() != QProcess::NotRunning || m_attempt >= 5)
        return;
    const QString pref = m_settings.value("transport", "auto").toString();
    QString target;
    if (pref != "usb" && m_wirelessReady)
        target = m_wirelessTarget;
    else if (pref != "wireless" && m_usbReady)
        target = m_settings.value("serial").toString();
    if (target.isEmpty()) {
        m_mirrorStatus = "Waiting for phone";
        emit changed();
        return;
    }
    if (!QFileInfo::exists(scrcpy())) {
        m_error = tr("scrcpy is missing. Install scrcpy.");
        m_mirrorStatus = "Unavailable";
        emit changed();
        return;
    }
    m_activeTarget = target;
    m_deliberateStop = false;
    m_mirrorStatus =
        target == m_settings.value("serial").toString() ? "Mirroring over USB" : "Mirroring over Wi-Fi";
    QStringList args{"--serial", target, "--window-title", "Phonelink — Phone screen"};
    if (!m_settings.value("audio", false).toBool())
        args << "--no-audio";
    record("screen-started", m_mirrorStatus);
    m_mirror.start(scrcpy(), args);
    m_stableTimer.start();
    emit changed();
}
void Backend::mirrorEnded(int code, QProcess::ExitStatus status) {
    if (m_shuttingDown)
        return;
    m_stableTimer.stop();
    if (m_deliberateStop || (code == 0 && status == QProcess::NormalExit)) {
        m_wanted = false;
        m_deliberateStop = false;
        m_mirrorStatus = "Stopped";
        record("screen-closed");
    } else {
        ++m_attempt;
        record("screen-failed",
               QString("exit=%1 status=%2 attempt=%3").arg(code).arg(int(status)).arg(m_attempt));
        if (m_wanted && m_settings.value("retry", true).toBool() && m_attempt < 5) {
            const int delay = retryDelay(m_attempt);
            m_mirrorStatus = tr("Retrying in %1 seconds").arg(delay / 1000);
            m_retryTimer.start(delay);
        } else {
            m_wanted = false;
            m_mirrorStatus = "Stopped after error";
            m_error = tr("Screen stopped after an error. Check history, then choose Open Screen to retry.");
        }
    }
    emit changed();
}
void Backend::record(const QString &event, const QString &detail) {
    const auto now = QDateTime::currentDateTimeUtc();
    QJsonObject entry{{"time", now.toString(Qt::ISODateWithMs)},
                      {"event", event},
                      {"detail", detail},
                      {"phoneSerial", m_settings.value("serial").toString()},
                      {"model", m_model},
                      {"android", m_android},
                      {"usbPresent", m_usbPresent},
                      {"usbDebug", m_usbStatus},
                      {"usbId", m_usbId},
                      {"wirelessDebug", m_wirelessStatus},
                      {"endpoint", m_endpoint},
                      {"activeTarget", m_activeTarget},
                      {"screen", m_mirrorStatus}};
    QFile file(m_history + "/" + now.toString("yyyy-MM-dd") + ".jsonl");
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        if (file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n') < 0)
            m_error = tr("Could not write connection history.");
    } else
        m_error = tr("Could not write connection history.");
    m_recent.prepend(now.toLocalTime().toString("HH:mm:ss") + "  " + event +
                     (detail.isEmpty() ? QString() : " · " + detail.left(300).replace('\n', ' ')));
    while (m_recent.size() > 40)
        m_recent.removeLast();
    emit changed();
}
