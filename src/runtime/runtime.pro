TARGET = Qt3DStudioRuntime2
MODULE = 3dstudioruntime2

QT += core-private 3dcore-private 3drender-private 3dinput 3dlogic 3danimation 3dextras qml 3dquick 3dquickscene2d

SOURCES += \
    q3dsuipparser.cpp \
    q3dsabstractxmlparser.cpp \
    q3dsutils.cpp \
    q3dsmeshloader.cpp \
    q3dsuippresentation.cpp \
    q3dsenummaps.cpp \
    q3dsmesh.cpp \
    q3dscustommaterial.cpp \
    q3dsmaterial.cpp \
    q3dseffect.cpp \
    q3dsdatamodelparser.cpp \
    q3dsdefaultmaterialgenerator.cpp \
    q3dstextrenderer.cpp \
    q3dstextmaterialgenerator.cpp \
    q3dsengine.cpp \
    q3dswindow.cpp \
    q3dsuipdocument.cpp \
    q3dsscenemanager.cpp \
    q3dsanimationmanager.cpp \
    q3dsuiaparser.cpp \
    q3dsprofiler.cpp \
    q3dscustommaterialgenerator.cpp \
    q3dsabstractdocument.cpp \
    q3dsuiadocument.cpp \
    q3dsqmldocument.cpp \
    q3dsslideplayer.cpp \
    q3dsimagemanager.cpp \
    q3dsbehavior.cpp \
    q3dsinputmanager.cpp \
    q3dsconsolecommands.cpp \
    q3dsinlineqmlsubpresentation.cpp \
    q3dslogging.cpp \
    q3dsviewportsettings.cpp

HEADERS += \
    q3dsruntimeglobal.h \
    q3dsruntimeglobal_p.h \
    q3dsuipparser_p.h \
    q3dsabstractxmlparser_p.h \
    q3dsutils_p.h \
    q3dsmeshloader_p.h \
    q3dsuippresentation_p.h \
    q3dsenummaps_p.h \
    q3dsmesh_p.h \
    q3dscustommaterial_p.h \
    q3dsmaterial_p.h \
    q3dseffect_p.h \
    q3dsdatamodelparser_p.h \
    q3dspresentationcommon_p.h \
    q3dsdefaultmaterialgenerator_p.h \
    q3dstextrenderer_p.h \
    q3dstextmaterialgenerator_p.h \
    q3dsgraphicslimits_p.h \
    q3dsengine_p.h \
    q3dswindow_p.h \
    q3dsuipdocument_p.h \
    q3dsscenemanager_p.h \
    q3dsanimationmanager_p.h \
    q3dsuiaparser_p.h \
    q3dsprofiler_p.h \
    q3dscustommaterialgenerator_p.h \
    q3dsabstractdocument_p.h \
    q3dsuiadocument_p.h \
    q3dsqmldocument_p.h \
    q3dsslideplayer_p.h \
    q3dsdatainputentry_p.h \
    q3dsimagemanager_p.h \
    q3dsimageloaders_p.h \
    q3dsbehavior_p.h \
    q3dsinputmanager_p.h \
    q3dsconsolecommands_p.h \
    q3dsinlineqmlsubpresentation_p.h \
    q3dslogging_p.h \
    q3dsviewportsettings_p.h

qtHaveModule(widgets) {
    QT += widgets
    SOURCES += \
        q3dsgraphexplorer.cpp
    HEADERS += \
        q3dsgraphexplorer_p.h
}

RESOURCES += \
    q3dsres.qrc

mingw: LIBS += -lpsapi

include(api/api.pri)
include(behaviorapi/behaviorapi.pri)
include(shadergenerator/shadergenerator.pri)
include(utils/utils.pri)

qtConfig(q3ds-profileui): include(profileui/profileui.pri)

include(doc/doc.pri)

load(qt_module)

