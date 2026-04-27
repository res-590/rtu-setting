#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QByteArray>
#include <QString>

class QButtonGroup;
class QLabel;
class QPushButton;
class QWidget;
class QFrame;
class QListWidget;
class QSplitter;
class QPlainTextEdit;
class QStackedWidget;
class BasicPage;
class yunxingcanshu;
class sensor;
class dtuset;
class portset;
class Device_connection;
class UpdateManager;

#define LOOCTET(b)              ((BYTE)(((BYTE)(b)) &0x0f))
#define HIOCTET(b)              ((BYTE)((((BYTE)(b)) >>4) &0x0f))
//extern void Base_BW(char Base[],int model,int CJys[],char TPzb[]);


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

typedef struct Base
{
    QByteArray CJ;
}Base;

typedef unsigned char       BYTE;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void appendSideLog(const QString &text);
    void clearSideLog();
public:
        static QByteArray CJ_data;
        static QString str;
        static QByteArray TPzb;

private slots:
    void updateConnectionStatus(bool connected);
    void triggerUpdateCheck();

private:
    enum PageIndex {
        BasicPageIndex = 0,
        RuntimePageIndex,
        SensorPageIndex,
        DtuPageIndex,
        PortPageIndex,
        ConnectionPageIndex
    };

    void setupShellUi();
    void setupChildPages();
    void switchToPage(int pageIndex);
    void applyShellStyle();

    Ui::MainWindow *ui;
    QListWidget *m_navList;
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLabel *m_statusLabel;
    QLabel *m_serialLabel;
    QPushButton *m_updateButton;
    QStackedWidget *m_pageStack;
    QPlainTextEdit *m_logView;
    BasicPage *m_basicPage;
    yunxingcanshu *m_runtimePage;
    sensor *m_sensorPage;
    dtuset *m_dtuPage;
    portset *m_portPage;
    Device_connection *m_connectionTool;
    UpdateManager *m_updateManager;
};
#endif // MAINWINDOW_H
