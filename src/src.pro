TEMPLATE = subdirs

SUBDIRS += \
    runtime \
    doc

qtHaveModule(quick) {
    SUBDIRS += imports
    imports.depends = runtime
}
