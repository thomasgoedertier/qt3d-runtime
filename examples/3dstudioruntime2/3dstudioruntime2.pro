TEMPLATE = subdirs

SUBDIRS += \
    simplewindow \
    simpleoffscreen

qtHaveModule(quick) {
    SUBDIRS += simpleqml
}

qtHaveModule(widgets) {
    SUBDIRS += simplewidget
}
