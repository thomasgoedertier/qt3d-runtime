#include(../examples.pri)

TEMPLATE = app

QT += widgets qml quick 3dstudioruntime2

integrity: DEFINES += USE_EMBEDDED_FONTS

target.path = $$[QT_INSTALL_EXAMPLES]/3dstudioruntime2/$$TARGET
INSTALLS += target

SOURCES += main.cpp

RESOURCES += \
    qmldatainput.qrc

OTHER_FILES += qml/qmldatainput/* \
               doc/src/* \
               doc/images/*

# Icon in case example is included in installer
exists(example.ico): RC_ICONS = example.ico
