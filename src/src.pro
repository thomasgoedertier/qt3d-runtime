TEMPLATE = subdirs

SUBDIRS += \
    runtime

qtHaveModule(quick) {
    SUBDIRS += imports
    imports.depends = runtime
}
