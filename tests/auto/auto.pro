TEMPLATE = subdirs

SUBDIRS += \
    uipparser \
    uiaparser \
    meshloader \
    materialparser \
    effectparser \
    uippresentation \
    slidedeck \
    behaviors \
    documents \
    slides \
    slideplayer \
    surfaceviewer \
    q3dslancelot

qtHaveModule(quick): SUBDIRS += studio3d

qtHaveModule(widgets): SUBDIRS += widget
