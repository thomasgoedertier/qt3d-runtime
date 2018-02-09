TEMPLATE = app

QT += widgets 3dstudioruntime2

SOURCES += \
    main.cpp

RESOURCES += simplewidget.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/3dstudioruntime2/$$TARGET
INSTALLS += target
