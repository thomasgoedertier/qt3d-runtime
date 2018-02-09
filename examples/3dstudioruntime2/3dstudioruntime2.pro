TEMPLATE = subdirs

qtHaveModule(quick) {
    SUBDIRS += simpleqml
}

qtHaveModule(widgets) {
    SUBDIRS += simplewidget
}
