QT       += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# PDF FlateDecode 解压用（MinGW 自带静态库，无额外运行时依赖）
LIBS += -lz
# 提醒音播放（PlaySoundW）
LIBS += -lwinmm
# 下面的保持原样不用动...

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    desktoppet.cpp \
    pdfextract.cpp \
    stargame.cpp \
    statsdialog.cpp

HEADERS += \
    desktoppet.h \
    pdfextract.h \
    stargame.h \
    statsdialog.h

FORMS += \
    desktoppet.ui

RESOURCES += \
    aipet1.qrc

TRANSLATIONS += \
    aipet1_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
