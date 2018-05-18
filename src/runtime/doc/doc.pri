build_online_docs: \
    QMAKE_DOCS = $$PWD/online/qt3dstudioruntime2.qdocconf
else: \
    QMAKE_DOCS = $$PWD/qt3dstudioruntime2.qdocconf

OTHER_FILES += $$PWD/src/*.qdoc
