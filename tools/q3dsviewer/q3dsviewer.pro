QT += 3dstudioruntime2-private

qtHaveModule(widgets):!boot2qt {
    QT += widgets
    DEFINES += Q3DSVIEWER_WIDGETS
    SOURCES += q3dsmainwindow.cpp
    HEADERS += q3dsmainwindow.h
}

SOURCES += main.cpp \
    q3dsremotedeploymentserver.cpp \
    q3dsremotedeploymentmanager.cpp

RC_ICONS = resources/images/3D-studio-viewer.ico
ICON = resources/images/viewer.icns
QMAKE_TARGET_DESCRIPTION = Qt 3D Studio Viewer

android {
    TEMPLATE = app
    TARGET = q3dsviewer
} else {
    load(qt_app)
}

CONFIG += app_bundle

HEADERS += \
    q3dsremotedeploymentserver.h \
    q3dsremotedeploymentmanager.h

RESOURCES += \
    resources.qrc
