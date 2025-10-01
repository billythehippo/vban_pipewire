QT = core

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        ../vban_common/jack_backend.cpp \
        ../vban_common/udp.cpp \
        ../vban_common/vban_client_list.cpp \
        ../vban_common/vban_functions.cpp \
        ../vban_common/zita-resampler/cresampler.cc \
        ../vban_common/zita-resampler/resampler-table.cc \
        ../vban_common/zita-resampler/resampler.cc \
        ../vban_common/zita-resampler/vresampler.cc \
        main.cpp

LIBS += \
        -lpthread \
        -ljack

INCLUDEPATH += \
        ../vban_common

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    ../vban_common/jack_backend.h \
    ../vban_common/popen2.h \
    ../vban_common/ringbuffer.h \
    ../vban_common/udp.h \
    ../vban_common/vban.h \
    ../vban_common/vban_client_list.h \
    ../vban_common/vban_functions.h \
    ../vban_common/zita-resampler/cresampler.h \
    ../vban_common/zita-resampler/resampler-table.h \
    ../vban_common/zita-resampler/resampler.h \
    ../vban_common/zita-resampler/vresampler.h
