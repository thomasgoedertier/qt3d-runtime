TEMPLATE = app

QT += 3dstudioruntime2

SOURCES += \
    main.cpp \
    window_manualupdate.cpp \
    window_autoupdate.cpp

HEADERS += \
    window_manualupdate.h \
    window_autoupdate.h

RESOURCES += simplewindow.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/3dstudioruntime2/$$TARGET
INSTALLS += target
