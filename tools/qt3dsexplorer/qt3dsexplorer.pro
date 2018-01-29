QT += core gui widgets 3dstudioruntime2-private

TARGET = qt3dsexplorer

SOURCES += \
    main.cpp \
    q3dsexplorermainwindow.cpp \
    slideexplorerwidget.cpp \
    sceneexplorerwidget.cpp

HEADERS += \
    q3dsexplorermainwindow.h \
    slideexplorerwidget.h \
    sceneexplorerwidget.h

include(qtpropertybrowser/qtpropertybrowser.pri)
load(qt_tool)
