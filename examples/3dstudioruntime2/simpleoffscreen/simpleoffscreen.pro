TEMPLATE = app
CONFIG += console
QT += 3dstudioruntime2

SOURCES += \
    main.cpp

RESOURCES += simpleoffscreen.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/3dstudioruntime2/$$TARGET
INSTALLS += target
