QT += core gui qml quick quickcontrols2 dbus network
CONFIG += c++17 release
TARGET = phonelink
TEMPLATE = app
HEADERS += src/backend.h src/tray.h src/theme.h src/translator.h
SOURCES += src/main.cpp src/backend.cpp src/tray.cpp src/theme.cpp src/translator.cpp
RESOURCES += src/resources.qrc
