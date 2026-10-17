QT += core
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = pdfextract_test

LIBS += -lz

SOURCES += \
    main_test.cpp \
    ../pdfextract.cpp

HEADERS += \
    ../pdfextract.h

INCLUDEPATH += ../
