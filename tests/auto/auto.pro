TEMPLATE = subdirs

SUBDIRS += \
    uipparser \
    uiaparser \
    meshloader \
    materialparser \
    effectparser \
    slides \
    slideplayer \
    q3dslancelot \
    documents

qtHaveModule(quick): SUBDIRS += studio3d

qtHaveModule(widgets): SUBDIRS += widget
