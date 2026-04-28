#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "basicpage.h"
#include "sensor.h"
#include "portset.h"
#include "dtuset.h"
#include "yunxingcanshu.h"
#include "device_connection.h"
#include "RTU_ParameterSetting.h"
#include "update_manager.h"
#include "app_metadata.h"

#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextCursor>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_navList(nullptr)
    , m_titleLabel(nullptr)
    , m_subtitleLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_serialLabel(nullptr)
    , m_updateButton(nullptr)
    , m_pageStack(nullptr)
    , m_logView(nullptr)
    , m_basicPage(nullptr)
    , m_runtimePage(nullptr)
    , m_sensorPage(nullptr)
    , m_dtuPage(nullptr)
    , m_portPage(nullptr)
    , m_connectionTool(nullptr)
    , m_updateManager(nullptr)
{
    ui->setupUi(this);
    m_RTU_ParameterSetting->m_MainWindow = this;
    setupShellUi();
    setupChildPages();
    applyShellStyle();
    updateConnectionStatus(false);

    m_updateManager = new UpdateManager(this, this);
    connect(m_updateManager, &UpdateManager::updateAvailabilityChanged,
            this, &MainWindow::updateUpdateButtonState);
    refreshUpdateButtonAppearance(false);
    m_updateManager->scheduleStartupCheck();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::appendSideLog(const QString &text)
{
    if (!m_logView || text.isEmpty()) {
        return;
    }

    m_logView->moveCursor(QTextCursor::End);
    m_logView->insertPlainText(text);
    m_logView->moveCursor(QTextCursor::End);
}

void MainWindow::clearSideLog()
{
    if (m_logView) {
        m_logView->clear();
    }
}

void MainWindow::setupShellUi()
{
    setWindowTitle(APP_DISPLAY_NAME_LITERAL);
    setMinimumSize(1320, 820);

    m_navList = ui->navListWidget;
    m_titleLabel = ui->pageTitleLabel;
    m_subtitleLabel = ui->pageSubtitleLabel;
    m_statusLabel = ui->statusValueLabel;
    m_serialLabel = ui->serialValueLabel;
    m_pageStack = ui->pageStackWidget;
    m_logView = ui->logPlainTextEdit;
    m_updateButton = new QPushButton(QStringLiteral("检查更新"), this);

    ui->logTitleLabel->setText(QStringLiteral("接收日志"));
    ui->statusTitleLabel->setText(QStringLiteral("设备连接状态"));
    if (m_logView) {
        m_logView->setPlainText(QStringLiteral("串口接收报文后，这里会实时显示接收内容和解析结果。\r\n"));
    }

    ui->headerFrame->setMinimumHeight(118);
    ui->ribbonFrame->setMinimumHeight(104);
    ui->ribbonFrame->setMaximumHeight(122);
    ui->pageHeaderFrame->setMinimumHeight(104);
    ui->pageTitleLabel->setWordWrap(true);
    ui->pageSubtitleLabel->setWordWrap(true);
    ui->serialValueLabel->setWordWrap(true);
    ui->statusValueLabel->setAlignment(Qt::AlignCenter);
    if (ui->verticalLayoutStatusCard != nullptr) {
        ui->verticalLayoutStatusCard->addWidget(m_updateButton);
    }

    connect(m_updateButton, &QPushButton::clicked, this, &MainWindow::triggerUpdateCheck);

    if (auto splitter = findChild<QSplitter*>(QStringLiteral("mainSplitter"))) {
        splitter->setStretchFactor(0, 5);
        splitter->setStretchFactor(1, 1);
        QList<int> sizes;
        sizes << 1120 << 340;
        splitter->setSizes(sizes);
    }

    m_navList->clear();
    m_navList->addItem(QStringLiteral("基础参数"));
    m_navList->addItem(QStringLiteral("测站操作"));
    m_navList->addItem(QStringLiteral("传感器参数"));
    m_navList->addItem(QStringLiteral("DTU 参数"));
    m_navList->addItem(QStringLiteral("端口参数"));
    m_navList->addItem(QStringLiteral("通信设置"));
    m_navList->setSpacing(10);
    m_navList->setGridSize(QSize(168, 60));
    m_navList->setWrapping(false);
    m_navList->setMovement(QListView::Static);
    m_navList->setCurrentRow(0);

    connect(m_navList, &QListWidget::currentRowChanged, this, &MainWindow::switchToPage);
}

void MainWindow::setupChildPages()
{
    m_basicPage = new BasicPage(m_pageStack);
    m_runtimePage = new yunxingcanshu(m_pageStack);
    m_sensorPage = new sensor(m_pageStack);
    m_dtuPage = new dtuset(m_pageStack);
    m_portPage = new portset(m_pageStack);
    m_connectionTool = new Device_connection(m_pageStack);

    m_RTU_ParameterSetting->m_basicPage = m_basicPage;
    m_RTU_ParameterSetting->m_runtimePage = m_runtimePage;
    m_RTU_ParameterSetting->m_sensorPage = m_sensorPage;
    m_RTU_ParameterSetting->m_dtuset = m_dtuPage;
    m_RTU_ParameterSetting->m_portset = m_portPage;
    m_RTU_ParameterSetting->m_Device_connection = m_connectionTool;

    m_pageStack->addWidget(m_basicPage);
    m_pageStack->addWidget(m_runtimePage);
    m_pageStack->addWidget(m_sensorPage);
    m_pageStack->addWidget(m_dtuPage);
    m_pageStack->addWidget(m_portPage);
    m_pageStack->addWidget(m_connectionTool);

    connect(m_connectionTool, &Device_connection::connectionStatusChanged,
            this, &MainWindow::updateConnectionStatus);
}

void MainWindow::switchToPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_pageStack->count()) {
        return;
    }

    m_pageStack->setCurrentIndex(pageIndex);

    QString pageTitle;
    QString pageSubtitle;

    switch (pageIndex) {
    case BasicPageIndex:
        pageTitle = QStringLiteral("基础参数");
        pageSubtitle = QStringLiteral("配置中心站地址、遥测站地址、工作模式、采集要素和定时报送计划。");
        break;
    case RuntimePageIndex:
        pageTitle = QStringLiteral("测站操作");
        pageSubtitle = QStringLiteral("集中处理测站查询、控制和维护操作，并在右侧直接显示接收结果。");
        break;
    case SensorPageIndex:
        pageTitle = QStringLiteral("传感器参数");
        pageSubtitle = QStringLiteral("管理传感器列表、关联端口、采集要素和告警阈值。");
        break;
    case DtuPageIndex:
        pageTitle = QStringLiteral("DTU 参数");
        pageSubtitle = QStringLiteral("配置内置 DTU 通讯参数和各通道接入信息。");
        break;
    case PortPageIndex:
        pageTitle = QStringLiteral("端口参数");
        pageSubtitle = QStringLiteral("统一管理各端口设备类型、波特率、RF 频点及保留字。");
        break;
    case ConnectionPageIndex:
        pageTitle = QStringLiteral("通信设置");
        pageSubtitle = QStringLiteral("完成串口连接、手动发报和运行日志查看，接收日志实时显示在右侧。");
        break;
    default:
        return;
    }

    m_titleLabel->setText(pageTitle);
    m_subtitleLabel->setText(pageSubtitle);
}

void MainWindow::applyShellStyle()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow{background:#f3f6fb;color:#1f2937;}"
        "QFrame#headerFrame,QFrame#contentShellFrame,QFrame#sideRailFrame,QFrame#pageHeaderFrame,"
        "QFrame#statusCard,QFrame#logFrame,QFrame#ribbonFrame{"
        "background:#ffffff;border:1px solid #d8e2f0;border-radius:10px;}"
        "QLabel#pageTitleLabel{font-size:24px;font-weight:700;color:#0f172a;min-height:34px;}"
        "QLabel#pageSubtitleLabel{font-size:14px;line-height:1.5;color:#475569;padding-top:2px;}"
        "QLabel#statusTitleLabel,QLabel#logTitleLabel{font-size:16px;font-weight:700;color:#0f172a;}"
        "QLabel#statusValueLabel{min-width:80px;min-height:34px;}"
        "QLabel#serialValueLabel{font-size:13px;color:#475569;}"
        "QListWidget#navListWidget{background:transparent;border:none;outline:none;padding:4px;}"
        "QListWidget#navListWidget::item{background:#eef4ff;border:1px solid #d8e7ff;border-radius:8px;"
        "padding:12px 18px;color:#1e3a5f;min-width:140px;min-height:24px;font-size:14px;font-weight:600;}"
        "QListWidget#navListWidget::item:selected{background:#2f78e8;border:1px solid #2f78e8;color:#ffffff;}"
        "QListWidget#navListWidget::item:hover{background:#dbeafe;border:1px solid #9ec5fe;}"
        "QPushButton{background:#2f78e8;color:#ffffff;border:none;border-radius:6px;padding:6px 14px;"
        "min-height:34px;font-size:14px;font-weight:600;}"
        "QPushButton:hover{background:#2468cc;}"
        "QPushButton:pressed{background:#1d57aa;}"
        "QPushButton:disabled{background:#b8c7db;color:#eef2f7;}"
        "QPushButton#BackButton{background:#ffffff;color:#2f78e8;border:1px solid #bfd4f6;}"
        "QPushButton#BackButton:hover{background:#edf4ff;}"
        "QPlainTextEdit,QTextEdit{background:#f8fbff;border:1px solid #d8e2f0;border-radius:8px;padding:6px;}"
        "QComboBox,QLineEdit,QSpinBox,QDoubleSpinBox,QDateTimeEdit{min-height:30px;border:1px solid #cbd5e1;"
        "border-radius:6px;padding:3px 8px;background:#ffffff;}"
    ));
}

void MainWindow::updateConnectionStatus(bool connected)
{
    if (!m_statusLabel || !m_serialLabel || !m_logView) {
        return;
    }

    m_statusLabel->setText(connected ? QStringLiteral("在线") : QStringLiteral("离线"));
    m_statusLabel->setStyleSheet(connected
                                     ? QStringLiteral("background:#e9f4ff;color:#0f5ea8;border:1px solid #bcd7f1;border-radius:12px;padding:6px 12px;font-size:16px;font-weight:700;")
                                     : QStringLiteral("background:#f3f6f9;color:#60717f;border:1px solid #d5dee8;border-radius:12px;padding:6px 12px;font-size:16px;font-weight:700;"));
    m_serialLabel->setText(connected ? QStringLiteral("串口：在线配置链路已建立")
                                     : QStringLiteral("串口：等待设备接入"));
}

void MainWindow::triggerUpdateCheck()
{
    if (m_updateManager != nullptr) {
        m_updateManager->checkForUpdates(true);
    }
}

void MainWindow::updateUpdateButtonState(bool available, const QString &version)
{
    refreshUpdateButtonAppearance(available, version);
}

void MainWindow::refreshUpdateButtonAppearance(bool available, const QString &version)
{
    if (m_updateButton == nullptr) {
        return;
    }

    if (available) {
        m_updateButton->setText(QStringLiteral("下载更新"));
        QString toolTip = QStringLiteral("检测到新版本");
        if (!version.trimmed().isEmpty()) {
            toolTip += QStringLiteral("：%1").arg(version.trimmed());
        }
        m_updateButton->setToolTip(toolTip);
        m_updateButton->setStyleSheet(QStringLiteral(
            "QPushButton{background:#d92d20;color:#ffffff;border:none;border-radius:6px;padding:6px 14px;min-height:34px;font-size:14px;font-weight:700;}"
            "QPushButton:hover{background:#b42318;}"
            "QPushButton:pressed{background:#912018;}"
            "QPushButton:disabled{background:#fda29b;color:#fff1f3;}"));
        return;
    }

    m_updateButton->setText(QStringLiteral("检查更新"));
    m_updateButton->setToolTip(QString());
    m_updateButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2f78e8;color:#ffffff;border:none;border-radius:6px;padding:6px 14px;min-height:34px;font-size:14px;font-weight:600;}"
        "QPushButton:hover{background:#2468cc;}"
        "QPushButton:pressed{background:#1d57aa;}"
        "QPushButton:disabled{background:#b8c7db;color:#eef2f7;}"));
}
