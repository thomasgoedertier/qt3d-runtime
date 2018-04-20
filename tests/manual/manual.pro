TEMPLATE = subdirs
qtHaveModule(widgets) {
    SUBDIRS += \
        qt3dsexplorer \
        datamodelgen
}
