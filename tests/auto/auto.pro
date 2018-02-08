TEMPLATE = subdirs

SUBDIRS += \
    uipparser \
    uiaparser \
    meshloader \
    materialparser \
    effectparser \
    slides \
    q3dslancelot \
    documents

qtHaveModule(quick): SUBDIRS += studio3d
