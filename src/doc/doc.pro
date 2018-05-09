TEMPLATE = aux

build_online_docs: \
    QMAKE_DOCS = $$PWD/online/Qt3DStudioRuntime2.qdocconf
else: \
    QMAKE_DOCS = $$PWD/Qt3DStudioRuntime2.qdocconf

OTHER_FILES += $$PWD/src/*.qdoc
