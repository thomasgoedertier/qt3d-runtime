TEMPLATE = subdirs
SUBDIRS += standalone
qtHaveModule(widgets) {
    SUBDIRS += \
        qt3dsexplorer \
        datamodelgen
}
