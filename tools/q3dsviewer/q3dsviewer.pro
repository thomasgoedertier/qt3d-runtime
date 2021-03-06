QT += 3dstudioruntime2-private

qtHaveModule(widgets):!boot2qt {
    QT += widgets
    DEFINES += Q3DSVIEWER_WIDGETS
    SOURCES += \
        q3dsmainwindow.cpp \
        q3dsaboutdialog.cpp
    HEADERS += \
        q3dsmainwindow.h \
        q3dsaboutdialog.h

    FORMS += q3dsaboutdialog.ui
}

SOURCES += main.cpp

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

RESOURCES += \
    resources.qrc

