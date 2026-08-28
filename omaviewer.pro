QT += core dbus gui network qml quick quickcontrols2 quickdialogs2 quick3d

CONFIG += c++17 release

TARGET = omaviewer
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
