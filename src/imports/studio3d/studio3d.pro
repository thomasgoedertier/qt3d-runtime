CXX_MODULE = qml
TARGET = q3dsstudio3dplugin
TARGETPATH = QtStudio3D.2
IMPORT_VERSION = 2.0

QT += qml quick 3drender-private 3dstudioruntime2-private

SOURCES += \
    plugin.cpp \
    q3dsstudio3ditem.cpp \
    q3dsstudio3drenderer.cpp \
    q3dsstudio3dnode.cpp \
    q3dspresentationitem.cpp \
    q3dssubpresentationsettings.cpp \
    q3dsviewersettings.cpp

HEADERS += \
    q3dsstudio3ditem_p.h \
    q3dsstudio3drenderer_p.h \
    q3dsstudio3dnode_p.h \
    q3dspresentationitem_p.h \
    q3dssubpresentationsettings_p.h \
    q3dsviewersettings_p.h

OTHER_FILES += \
    qmldir

load(qml_plugin)
