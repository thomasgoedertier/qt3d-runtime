QT += 3dstudioruntime2-private

qtHaveModule(widgets) {
    QT += widgets
    DEFINES += Q3DSVIEWER_WIDGETS
    SOURCES += q3dsmainwindow.cpp
    HEADERS += q3dsmainwindow.h
}

SOURCES += main.cpp

load(qt_app)

CONFIG += app_bundle
