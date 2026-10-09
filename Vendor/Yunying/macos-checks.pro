include(yunying.pro)
QT += testlib
TARGET = FenliuYunyingChecks
QMAKE_INFO_PLIST =
DEFINES += LOCAL_CHECKS
OBJECTIVE_SOURCES -= src/credentialstore.mm
SOURCES += tests/macchecks.cpp tests/fakecredentialstore.cpp
