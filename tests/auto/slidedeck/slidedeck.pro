QT += testlib 3dstudioruntime2-private
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
           tst_slidedeck.cpp

RESOURCES += \
    resources.qrc
