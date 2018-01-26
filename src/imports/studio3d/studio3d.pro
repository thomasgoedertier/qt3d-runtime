CXX_MODULE = qml
TARGET = q3dsstudio3dplugin
TARGETPATH = QtStudio3D.2
IMPORT_VERSION = 2.0

QT += qml quick 3dstudioruntime2

SOURCES += \
    plugin.cpp \
    q3dsstudio3ditem.cpp

HEADERS += \
    q3dsstudio3ditem_p.h

OTHER_FILES += \
    qmldir

load(qml_plugin)
