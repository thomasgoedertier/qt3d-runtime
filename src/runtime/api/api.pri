SOURCES += \
    $$PWD/q3dspresentation.cpp \
    $$PWD/q3dspresentationcontroller.cpp \
    $$PWD/q3dssurfaceviewer.cpp \
    $$PWD/q3dsdatainput.cpp \
    $$PWD/q3dselement.cpp \
    $$PWD/q3dssceneelement.cpp \
    $$PWD/q3dsviewersettings.cpp

HEADERS += \
    $$PWD/q3dspresentation.h \
    $$PWD/q3dspresentation_p.h \
    $$PWD/q3dssurfaceviewer.h \
    $$PWD/q3dssurfaceviewer_p.h \
    $$PWD/q3dsdatainput.h \
    $$PWD/q3dsdatainput_p.h \
    $$PWD/q3dselement.h \
    $$PWD/q3dselement_p.h \
    $$PWD/q3dssceneelement.h \
    $$PWD/q3dssceneelement_p.h \
    $$PWD/q3dsviewersettings.h \
    $$PWD/q3dsviewersettings_p.h

qtHaveModule(widgets) {
    SOURCES += \
        $$PWD/q3dswidget.cpp
    HEADERS += \
        $$PWD/q3dswidget.h \
        $$PWD/q3dswidget_p.h
}
