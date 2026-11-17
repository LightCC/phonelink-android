QT += core gui quick quickcontrols2 testlib dbus network
CONFIG += c++17 testcase
TARGET = phonelink_tests
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/tray.h ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += ../src/tray.cpp phonelink_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc
