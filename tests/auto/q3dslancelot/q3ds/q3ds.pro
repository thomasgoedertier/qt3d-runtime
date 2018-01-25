CONFIG += testcase
TARGET = tst_q3ds
DESTDIR=..
macos:CONFIG -= app_bundle
CONFIG += console

SOURCES += tst_q3ds.cpp

include(../baseline/qbaselinetest.pri)

uipprojects.files = ../data
uipprojects.path = $${DESTDIR}
COPIES += uipprojects
