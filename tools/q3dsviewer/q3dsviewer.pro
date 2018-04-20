QT += 3dstudioruntime2-private

qtHaveModule(widgets) {
    QT += widgets
    DEFINES += Q3DSVIEWER_WIDGETS
    SOURCES += q3dsmainwindow.cpp
    HEADERS += q3dsmainwindow.h
}

SOURCES += main.cpp

RC_ICONS = resources/images/3D-studio-viewer.ico
ICON = resources/images/viewer.icns
QMAKE_TARGET_DESCRIPTION = Qt 3D Studio Viewer

load(qt_app)

CONFIG += app_bundle
