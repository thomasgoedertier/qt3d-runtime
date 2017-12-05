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
    q3dsdefaultmaterialgenerator.cpp \
    q3dstextrenderer.cpp \
    q3dstextmaterialgenerator.cpp \
    q3dswindow.cpp \
    q3dsuipdocument.cpp \
    q3dsscenemanager.cpp \
    q3dsanimationmanager.cpp \
    q3dsuiaparser.cpp \
    q3dsprofiler.cpp \
    q3dscustommaterialgenerator.cpp

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
    q3dsdefaultmaterialgenerator.h \
    q3dstextrenderer.h \
    q3dstextmaterialgenerator.h \
    q3dsgraphicslimits.h \
    q3dswindow.h \
    q3dsuipdocument.h \
    q3dsscenemanager.h \
    q3dsanimationmanager.h \
    q3dsuiaparser.h \
    q3dsprofiler_p.h \
    q3dscustommaterialgenerator.h

RESOURCES += \
    q3dsres.qrc

mingw: LIBS += -lpsapi

include(shadergenerator/shadergenerator.pri)
include(profileui/profileui.pri)

load(qt_module)
