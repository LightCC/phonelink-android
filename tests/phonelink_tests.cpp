#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include "backend.h"
#include "tray.h"
#include "theme.h"
#include "translator.h"
class PhonelinkTests:public QObject {
 Q_OBJECT
 QTemporaryDir root;
 void write(const QString &name,const QByteArray &data){QFile f(root.filePath(name));QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(data),qint64(data.size()));}
 int launches(){QFile f(root.filePath("launches"));return f.open(QIODevice::ReadOnly)?f.readAll().toInt():0;}
 QJsonObject profiles(Backend &b){QFile f(b.historyDirectory()+"/phones.json");if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
private slots:
 void initTestCase(){
    QVERIFY(root.isValid());
    qputenv("XDG_STATE_HOME",root.filePath("state").toUtf8());
    QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,root.filePath("config"));
    qputenv("PHONELINK_FAKE_ROOT",root.path().toUtf8());
    const QString source=QFINDTESTDATA("fake-adb.py");QVERIFY(!source.isEmpty());
    qputenv("PHONELINK_ADB",source.toUtf8());qputenv("PHONELINK_SCRCPY",QFINDTESTDATA("fake-scrcpy.py").toUtf8());
 }
 void init(){
    QSettings s("omarchy","phonelink");s.clear();s.setValue("serial","TESTPHONE");s.setValue("bootstrap",false);s.sync();
    for(const auto &file:QStringList{"connected","available","usb","identity","launches","arguments","android"})QFile::remove(root.filePath(file));
    QDir(root.filePath("state")).removeRecursively();
    write("mode","hold");write("endpoint","192.168.1.50:34567");
 }
 void trayPixelsRenderWithoutInstalledIcons(){
    Backend b(nullptr,false);QObject owner;Tray tray(&owner,&b);
    QCOMPARE(tray.category(),QString("SystemServices"));QVERIFY(tray.iconName().isEmpty());
    const auto offline=tray.iconPixmap();QCOMPARE(offline.size(),3);
    for(const auto &p:offline){
        QCOMPARE(p.bytes.size(),p.width*p.height*4);
        int opaque=0;for(int i=0;i<p.bytes.size();i+=4)if(uchar(p.bytes[i])>0)++opaque;
        QVERIFY(opaque>p.width*p.height/4);
        QCOMPARE(uchar(p.bytes[0]),uchar(0));
    }
    write("available","1");b.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(b.state().value("wirelessReady").toBool(),10000);
    QVERIFY(tray.iconPixmap()[1].bytes!=offline[1].bytes);
    const auto connected=tray.iconPixmap();QSignalSpy changed(&tray,&Tray::NewIcon);
    b.Start();QTRY_VERIFY_WITH_TIMEOUT(b.state().value("running").toBool(),10000);
    QVERIFY(tray.iconPixmap()[1].bytes!=connected[1].bytes);QVERIFY(changed.count()>0);
    QVERIFY(offline[1].bytes.contains(QByteArray::fromHex("ffaeb9c8")));b.Stop();
 }
 void expiredPluginLeaseRestoresTray(){
    QTemporaryDir dir;const QString path=dir.filePath("lease");
    auto writeExpiry=[&](qint64 expiry){QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray::number(expiry));};
    QVERIFY(!Tray::pluginLeaseActive(path));
    writeExpiry(QDateTime::currentSecsSinceEpoch()+25);QVERIFY(Tray::pluginLeaseActive(path));
    writeExpiry(QDateTime::currentSecsSinceEpoch()-1);QVERIFY(!Tray::pluginLeaseActive(path));
    writeExpiry(QDateTime::currentSecsSinceEpoch()+3600);QVERIFY(!Tray::pluginLeaseActive(path));
 }
 void endpointsArePrivateAndValid(){
    QVERIFY(Backend::validEndpoint("192.168.1.50:34567"));QVERIFY(Backend::validEndpoint("100.118.49.98:30001"));
    QVERIFY(Backend::validEndpoint("172.16.1.2:65535"));QVERIFY(Backend::validEndpoint("10.0.0.1:1"));
    for(const auto &e:QStringList{"8.8.8.8:5555","192.168.1.1:0","192.168.1.1:65536","localhost:5555","192.168.1.1:22;echo x","172.32.0.1:5555"})QVERIFY(!Backend::validEndpoint(e));
 }
 void parsesAdbStates(){
    auto d=Backend::parseDevices("List of devices attached\nTESTPHONE device usb:1-1\nother unauthorized\nold offline\n");
    QCOMPARE(d.value("TESTPHONE"),QString("device"));QCOMPARE(d.value("other"),QString("unauthorized"));QCOMPARE(d.size(),3);
 }
 void remembersIdentityAndBothTransports(){
    write("usb","1");write("available","1");Backend b;
    QTRY_VERIFY_WITH_TIMEOUT(b.state().value("wirelessReady").toBool() && !b.busy(),10000);
    const auto phone=profiles(b).value("TESTPHONE").toObject();QCOMPARE(phone.value("model").toString(),QString("Example Phone"));
    QCOMPARE(phone.value("android").toString(),QString("17"));
    const auto c=phone.value("connections").toObject();QVERIFY(c.contains("usb:TESTPHONE"));QVERIFY(c.contains("192.168.1.50:34567"));
    QCOMPARE(c.value("192.168.1.50:34567").toObject().value("port").toInt(),34567);
    QVERIFY(!c.value("192.168.1.50:34567").toObject().value("firstSeen").toString().isEmpty());
    Backend restored(nullptr,false);QVERIFY(restored.state().value("profiles").toMap().contains("TESTPHONE"));
 }
 void newEndpointsAndPhoneVersionsPreserveHistory(){
    write("available","1");Backend b;
    QTRY_VERIFY_WITH_TIMEOUT(b.state().value("wirelessReady").toBool() && !b.busy(),10000);
    write("endpoint","192.168.1.50:45678");write("android","18");b.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(profiles(b).value("TESTPHONE").toObject().value("android").toString(),QString("18"),10000);
    const auto connections=profiles(b).value("TESTPHONE").toObject().value("connections").toObject();
    QVERIFY(connections.contains("192.168.1.50:34567"));QVERIFY(connections.contains("192.168.1.50:45678"));
    QDir directory(b.historyDirectory());QByteArray history;
    for(const auto &name:directory.entryList({"*.jsonl"})){QFile file(directory.filePath(name));QVERIFY(file.open(QIODevice::ReadOnly));history+=file.readAll();}
    QVERIFY(history.contains("phone-metadata-changed"));QVERIFY(history.contains("\\\"android\\\":\\\"17\\\""));
 }
 void wrongPhoneIsRejected(){
    write("available","1");write("identity","OTHERPHONE");Backend b;
    QTRY_VERIFY_WITH_TIMEOUT(!b.busy() && b.state().value("wireless").toString()=="identity not verified",10000);
    QVERIFY(!b.state().value("wirelessReady").toBool());QVERIFY(!profiles(b).contains("OTHERPHONE"));
    b.Start();QTest::qWait(500);QCOMPARE(launches(),0);
 }
 void closeDoesNotRestart(){
    write("available","1");write("mode","close");Backend b;b.Start();
    QTRY_COMPARE_WITH_TIMEOUT(launches(),1,10000);QTRY_VERIFY(!b.state().value("wanted").toBool());
    QTest::qWait(1300);QCOMPARE(launches(),1);
 }
 void crashRetriesAndManualStopCancels(){
    write("available","1");write("mode","once");Backend b;b.Start();
    QTRY_COMPARE_WITH_TIMEOUT(launches(),2,10000);QVERIFY(b.state().value("running").toBool());
    b.Stop();QTRY_VERIFY(!b.state().value("running").toBool());QTest::qWait(1300);QCOMPARE(launches(),2);
    QFile args(root.filePath("arguments"));QVERIFY(args.open(QIODevice::ReadOnly));QVERIFY(args.readAll().contains("--no-audio"));
 }
 void retryStopsAfterFiveFailures(){
    write("available","1");write("mode","crash");Backend b;b.Start();
    QTRY_COMPARE_WITH_TIMEOUT(launches(),5,25000);QTRY_VERIFY(!b.state().value("wanted").toBool());
    QTest::qWait(600);QCOMPARE(launches(),5);
 }
 void usbCanRestoreModernWireless(){
    write("usb","1");QSettings s("omarchy","phonelink");s.setValue("bootstrap",true);s.sync();Backend b;b.Start();
    QTRY_VERIFY_WITH_TIMEOUT(b.state().value("wirelessReady").toBool(),10000);
    QTRY_COMPARE_WITH_TIMEOUT(launches(),1,10000);QCOMPARE(b.state().value("target").toString(),QString("192.168.1.50:34567"));
    b.Stop();
 }
 void themeAndLanguage(){
    QTemporaryDir d;Theme theme(d.path());QCOMPARE(theme.accent(),QString("#FFD60A"));
    QFile lang(d.filePath("language"));QVERIFY(lang.open(QIODevice::WriteOnly));lang.write("da\n");lang.close();
    Translator translator(d.filePath("language"),":/i18n");QCOMPARE(translator.language(),QString("da"));QCOMPARE(translator.translate("Main","Settings"),QString::fromUtf8("Indstillinger"));
 }
 void startupOptionsAreIndependent(){
    Backend b(nullptr,false);
    QVERIFY(b.savePreferences("TESTPHONE","","auto",true,false,false,false,true));
    QVERIFY(b.preferences().value("showPanel").toBool());QVERIFY(!b.preferences().value("autoOpen").toBool());
 }
 void panelLoadsAndCanBeHidden(){
    Backend b(nullptr,false);QTemporaryDir d;Theme theme(d.path());Translator translator(d.filePath("language"),":/i18n");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend",&b);engine.rootContext()->setContextProperty("theme",&theme);
    engine.rootContext()->setContextProperty("translator",&translator);engine.rootContext()->setContextProperty("startHidden",false);
    engine.load(QUrl("qrc:/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
    auto *w=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(w);QVERIFY(w->isVisible());
    QTest::qWait(200);QVERIFY(!w->grabWindow().isNull());
    auto *grid=w->findChild<QQuickItem*>("statusGrid");auto *button=w->findChild<QQuickItem*>("screenButton");
    auto *toggle=w->findChild<QQuickItem*>("navigationToggle");auto *settingsNav=w->findChild<QQuickItem*>("settingsNavigation");
    QVERIFY(grid);QVERIFY(button);QVERIFY(toggle);QVERIFY(settingsNav);
    w->resize(650,390);QTRY_COMPARE(grid->property("columns").toInt(),3);
    w->resize(460,320);QTRY_COMPARE(grid->property("columns").toInt(),2);
    w->resize(300,280);QTRY_COMPARE(grid->property("columns").toInt(),1);
    QVERIFY(button->mapToScene(QPointF()).y()<100);
    QVERIFY(button->mapToScene(QPointF(button->width(),button->height())).y()<w->height());
    w->resize(650,390);QTest::qWait(100);
    QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,toggle->mapToScene(QPointF(toggle->width()/2,toggle->height()/2)).toPoint());
    QTRY_VERIFY(w->property("navigationExpanded").toBool());QCOMPARE(w->property("selectedTab").toInt(),0);
    QTRY_VERIFY(settingsNav->width()>100);QTest::qWait(200);
    QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,settingsNav->mapToScene(QPointF(settingsNav->width()/2,settingsNav->height()/2)).toPoint());
    QTRY_COMPARE(w->property("selectedTab").toInt(),1);
    QTest::keyClick(w,Qt::Key_B,Qt::ControlModifier);QTRY_VERIFY(!w->property("navigationExpanded").toBool());
    QTest::keyClick(w,Qt::Key_Escape);QTRY_VERIFY(!w->isVisible());
 }
};
int main(int argc,char **argv){QGuiApplication app(argc,argv);app.setApplicationName("phonelink-tests");QQuickStyle::setStyle("Material");PhonelinkTests t;return QTest::qExec(&t,argc,argv);}
#include "phonelink_tests.moc"
