TEMPLATE = app

QT += qml quick 3dstudioruntime2

SOURCES += \
    main.cpp

RESOURCES += simpleqml.qrc

OTHER_FILES += \
    main.qml

target.path = $$[QT_INSTALL_EXAMPLES]/3dstudioruntime2/$$TARGET
INSTALLS += target
