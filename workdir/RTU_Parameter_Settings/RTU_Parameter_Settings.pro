QT       += core gui
QT       += serialport network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = RTU_Parameter_Config_Tool
RC_ICONS = branding/app_icon.ico

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    basicpage.cpp \
    bwprotocol.cpp \
    dataquerypage.cpp \
    device_connection.cpp \
    dtuset.cpp \
    main.cpp \
    mainwindow.cpp \
    portset.cpp \
    sensor.cpp \
    src/RTU_ParameterSetting.cpp \
    update_manager.cpp \
    yunxingcanshu.cpp

HEADERS += \
    RTU_ParameterSetting.h \
    app_metadata.h \
    basicpage.h \
    bwprotocol.h \
    dataquerypage.h \
    device_connection.h \
    dtuset.h \
    mainwindow.h \
    portset.h \
    sensor.h \
    update_manager.h \
    yunxingcanshu.h

FORMS += \
    basicpage.ui \
    device_connection.ui \
    dtuset.ui \
    mainwindow.ui \
    portset.ui \
    sensor.ui \
    yunxingcanshu.ui

RESOURCES += \
    resources.qrc

DISTFILES += \
    dashboard_dark.qss

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
