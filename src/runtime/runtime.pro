TARGET = Qt3DStudioRuntime2
MODULE = 3dstudioruntime2

QT += widgets 3dcore-private 3drender 3dinput 3dlogic 3danimation 3dextras

SOURCES += \
    q3dsuipparser.cpp \
    q3dsabstractxmlparser.cpp \
    q3dsutils.cpp \
    q3dsmeshloader.cpp \
    q3dspresentation.cpp \
    q3dsenummaps.cpp \
    q3dsmesh.cpp \
    q3dscustommaterial.cpp \
    q3dsmaterial.cpp \
    q3dseffect.cpp \
    q3dsdatamodelparser.cpp \
    q3dsgraphexplorer.cpp \
    q3dsscenebuilder.cpp \
    q3dsanimationbuilder.cpp \
    q3dsdefaultmaterialgenerator.cpp \
    q3dstextrenderer.cpp \
    q3dstextmaterialgenerator.cpp \
    q3dswindow.cpp

HEADERS += \
    q3dsuipparser.h \
    q3dsabstractxmlparser.h \
    q3dsutils.h \
    q3dsmeshloader.h \
    q3dspresentation.h \
    q3dsenummaps.h \
    q3dsmesh.h \
    q3dscustommaterial.h \
    q3dsmaterial.h \
    q3dseffect.h \
    q3dsdatamodelparser.h \
    q3dsgraphexplorer.h \
    q3dspresentationcommon.h \
    q3dsscenebuilder.h \
    q3dsanimationbuilder.h \
    q3dsdefaultmaterialgenerator.h \
    q3dstextrenderer.h \
    q3dstextmaterialgenerator.h \
    q3dsgraphicslimits.h \
    q3dswindow.h

RESOURCES += \
    q3dsres.qrc

include(shadergenerator/shadergenerator.pri)

load(qt_module)
