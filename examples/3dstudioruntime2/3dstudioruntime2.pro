TEMPLATE = subdirs

SUBDIRS += \
    simplewindow \
    simpleoffscreen

qtHaveModule(quick) {
    SUBDIRS += simpleqml \
               qmldatainput
}

qtHaveModule(widgets) {
    SUBDIRS += simplewidget
}
