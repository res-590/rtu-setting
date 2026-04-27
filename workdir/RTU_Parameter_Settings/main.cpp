#include "mainwindow.h"
#include "RTU_ParameterSetting.h"
#include "app_metadata.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(APP_NAME_LITERAL);
    a.setApplicationDisplayName(APP_DISPLAY_NAME_LITERAL);
    a.setApplicationVersion(APP_VERSION_LITERAL);
    a.setWindowIcon(QIcon(QStringLiteral(":/branding/app_icon.ico")));

    QFile themeFile(QStringLiteral("dashboard_dark.qss"));
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&themeFile);
        a.setStyleSheet(stream.readAll());
    }

    m_RTU_ParameterSetting = new RTU_ParameterSetting;
    if (m_RTU_ParameterSetting->m_MainWindow == nullptr) {
        m_RTU_ParameterSetting->m_MainWindow = new MainWindow;
        if (m_RTU_ParameterSetting->m_MainWindow == nullptr) {
            qDebug() << "MainWindow create failed!";
        } else {
            m_RTU_ParameterSetting->m_MainWindow->show();
        }
    } else {
        m_RTU_ParameterSetting->m_MainWindow->show();
    }

    return a.exec();
}
