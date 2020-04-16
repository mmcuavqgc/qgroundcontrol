QT += multimedia quick qml core
CONFIG += c++11

RESOURCES += \
    $$PWD/transceiver.qrc \

HEADERS += \
    $$PWD/transceivermanager.h \
    $$PWD/radioprovider.h \
    $$PWD/radiomemberbase.h

SOURCES += \
    $$PWD/transceivermanager.cpp \
    $$PWD/radioprovider.cpp \
    $$PWD/radiomemberbase.cpp


WindowsBuild {
    HEADERS += \
        $$PWD/radiomember.h
    SOURCES += \
        $$PWD/radiomember.cpp
}
AndroidBuild {
    HEADERS += \
        $$PWD/androidraduimember.h
    SOURCES += \
        $$PWD/androidraduimember.cpp
}
