TEMPLATE = subdirs
SUBDIRS = q3ds scenegrabber

qtHaveModule(studio3d) {
    SUBDIRS += oldscenegrabber
}

