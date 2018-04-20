SOURCES += \
    $$PWD/q3dspresentation.cpp \
    $$PWD/q3dspresentationcontroller.cpp \
    $$PWD/q3dssurfaceviewer.cpp \
    $$PWD/q3dsdatainput.cpp

HEADERS += \
    $$PWD/q3dspresentation.h \
    $$PWD/q3dspresentation_p.h \
    $$PWD/q3dssurfaceviewer.h \
    $$PWD/q3dssurfaceviewer_p.h \
    $$PWD/q3dsdatainput.h \
    $$PWD/q3dsdatainput_p.h

qtHaveModule(widgets) {
    SOURCES += \
        $$PWD/q3dswidget.cpp
    HEADERS += \
        $$PWD/q3dswidget.h \
        $$PWD/q3dswidget_p.h
}
