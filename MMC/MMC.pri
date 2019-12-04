include(qtavplayer/qtavplayer.pri)
include(transceiver/transceiver.pri)
include(fpv/fpv.pri)
#include(MMCMount/MMCMount.pri)
#include(MMCKeys/MMCKeys.pri)

HEADERS += \
    $$PWD/mmcplugin.h \
    $$PWD/mmctools.h \
    $$PWD/dumpfile.h

SOURCES += \
    $$PWD/mmctools.cpp
