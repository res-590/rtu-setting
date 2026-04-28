#include "yunxingcanshu.h"
#include "ui_yunxingcanshu.h"
#include "RTU_ParameterSetting.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QComboBox>
#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kUpgradePacketLen = 980;

constexpr uint8_t kControlSyn = 0x16;

uint16_t nextUpgradeSerial()
{
    static uint16_t serial = 1;
    const uint16_t current = serial;
    ++serial;
    if (serial == 0) {
        serial = 1;
    }
    return current;
}

uint8_t makePackedByte(uint8_t high, uint8_t low)
{
    return static_cast<uint8_t>((low & 0x0f) | ((high & 0x0f) << 4));
}

QString operationTitle(int id)
{
    switch (id) {
    case 0:
        return QStringLiteral("查询测站时间");
    case 1:
        return QStringLiteral("校准测站时间");
    case 2:
        return QStringLiteral("查询测站版本");
    case 3:
        return QStringLiteral("查询测站工况");
    case 4:
        return QStringLiteral("输入端口状态配置");
    case 5:
        return QStringLiteral("输入端口状态查询");
    case 6:
        return QStringLiteral("输出端口电源控制");
    case 7:
        return QStringLiteral("输出端口状态查询");
    case 8:
        return QStringLiteral("远程重启测站");
    case 9:
        return QStringLiteral("恢复出厂设置");
    case 10:
        return QStringLiteral("格式化SD卡");
    case 11:
        return QStringLiteral("远程升级");
    default:
        return QStringLiteral("未命名操作");
    }
}

QString portStatusText(uint8_t status)
{
    switch (status) {
    case 0:
        return QStringLiteral("关闭");
    case 1:
        return QStringLiteral("开启");
    default:
        return QStringLiteral("状态%1").arg(status);
    }
}

QString yesNoText(bool enabled)
{
    return enabled ? QStringLiteral("正常") : QStringLiteral("告警");
}

QLabel *createResultLabel(const QString &text = QStringLiteral("--"))
{
    QLabel *label = new QLabel(text);
    label->setWordWrap(true);
    label->setMargin(4);
    label->setMinimumHeight(36);
    label->setStyleSheet(QStringLiteral(
        "background:#e9edf2;"
        "border:1px solid #dde4ec;"
        "border-radius:6px;"
        "color:#475467;"
        "padding:2px 4px;"));
    return label;
}

QWidget *wrapControlWithResult(QWidget *controls, QLabel *resultLabel)
{
    QWidget *container = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 6, 0, 6);
    layout->setSpacing(6);
    layout->addWidget(controls);
    layout->addWidget(resultLabel);
    return container;
}

QString decodeBcdDateTime(const QByteArray &payload)
{
    if (payload.size() < 6) {
        return {};
    }

    const auto bcd = [](char value) -> int {
        return BCD2Hex(static_cast<uint8_t>(value));
    };

    return QStringLiteral("20%1-%2-%3 %4:%5:%6")
        .arg(bcd(payload.at(0)), 2, 10, QLatin1Char('0'))
        .arg(bcd(payload.at(1)), 2, 10, QLatin1Char('0'))
        .arg(bcd(payload.at(2)), 2, 10, QLatin1Char('0'))
        .arg(bcd(payload.at(3)), 2, 10, QLatin1Char('0'))
        .arg(bcd(payload.at(4)), 2, 10, QLatin1Char('0'))
        .arg(bcd(payload.at(5)), 2, 10, QLatin1Char('0'));
}
}

yunxingcanshu::yunxingcanshu(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::yunxingcanshu)
{
    ui->setupUi(this);

    ui->summaryCard->hide();
    ui->actionCard->hide();
    ui->tableTitleLabel->hide();
    ui->tableHintLabel->hide();
    ui->labelListCountTitle->hide();
    ui->labelListCountValue->hide();
    setWindowTitle(QStringLiteral("测站操作"));

    for (QComboBox *combo : findChildren<QComboBox *>()) {
        combo->setMinimumHeight(30);
    }
    for (QLineEdit *edit : findChildren<QLineEdit *>()) {
        edit->setMinimumHeight(30);
    }

    setupOperationTable();
}

yunxingcanshu::~yunxingcanshu()
{
    delete ui;
}

void yunxingcanshu::setupOperationTable()
{
    ui->tableWidget->clear();
    ui->tableWidget->setColumnCount(2);
    ui->tableWidget->setRowCount(12);
    ui->tableWidget->setHorizontalHeaderLabels(QStringList()
                                               << QStringLiteral("操作")
                                               << QStringLiteral("结果"));
    ui->tableWidget->setHorizontalHeaderLabels(QStringList()
                                               << QStringLiteral("操作")
                                               << QStringLiteral("结果"));
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->tableWidget->setFocusPolicy(Qt::NoFocus);
    ui->tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->tableWidget->setShowGrid(false);
    ui->tableWidget->setAlternatingRowColors(false);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(64);

    const QList<OperationId> operations = {
        QueryTimeOperation,
        AdjustTimeOperation,
        QueryVersionOperation,
        QueryStatusOperation,
        InputPortConfigOperation,
        InputPortQueryOperation,
        OutPortControlOperation,
        OutPortQueryOperation,
        RestartStationOperation,
        RestoreFactoryOperation,
        FormatSdOperation,
        UpgradeOperation
    };

    for (int row = 0; row < operations.size(); ++row) {
        const OperationId id = operations.at(row);
        ui->tableWidget->setCellWidget(row, 0, createOperationButton(id));
        ui->tableWidget->setCellWidget(row, 1, createResultWidget(id));
        const bool expandedRow =
            id == InputPortConfigOperation ||
            id == OutPortControlOperation ||
            id == UpgradeOperation;
        ui->tableWidget->setRowHeight(row, expandedRow ? 92 : 64);
    }
}

QWidget *yunxingcanshu::createOperationButton(OperationId id)
{
    QPushButton *button = new QPushButton;
    switch (id) {
    case QueryTimeOperation: button->setText(QStringLiteral("查询测站时间")); break;
    case AdjustTimeOperation: button->setText(QStringLiteral("校准测站时间")); break;
    case QueryVersionOperation: button->setText(QStringLiteral("查询测站版本")); break;
    case QueryStatusOperation: button->setText(QStringLiteral("查询测站工况")); break;
    case InputPortConfigOperation: button->setText(QStringLiteral("输入端口状态配置")); break;
    case InputPortQueryOperation: button->setText(QStringLiteral("输入端口状态查询")); break;
    case OutPortControlOperation: button->setText(QStringLiteral("输出端口电源控制")); break;
    case OutPortQueryOperation: button->setText(QStringLiteral("输出端口状态查询")); break;
    case RestartStationOperation: button->setText(QStringLiteral("远程重启测站")); break;
    case RestoreFactoryOperation: button->setText(QStringLiteral("恢复出厂设置")); break;
    case FormatSdOperation: button->setText(QStringLiteral("格式化 SD 卡")); break;
    case UpgradeOperation: button->setText(QStringLiteral("远程升级")); break;
    default: button->setText(operationTitle(id)); break;
    }
    button->setMinimumWidth(176);
    const bool expandedButton =
        id == InputPortConfigOperation ||
        id == OutPortControlOperation ||
        id == UpgradeOperation;
    if (expandedButton) {
        button->setFixedHeight(84);
    } else {
        button->setFixedHeight(40);
    }
    button->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2f78e8;color:#ffffff;border:none;border-radius:8px;padding:7px 12px;font-weight:600;font-size:14px;}"
        "QPushButton:hover{background:#2468cc;}"
        "QPushButton:pressed{background:#1d57aa;}"));

    connect(button, &QPushButton::clicked, this, [this, id]() {
        triggerOperation(id);
    });

    m_rows[id].actionButton = button;
    QWidget *container = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 6, 8, 6);
    layout->setSpacing(0);
    layout->addWidget(button);
    return container;
}

QWidget *yunxingcanshu::createResultWidget(OperationId id)
{
    switch (id) {
    case InputPortConfigOperation:
        return createInputPortConfigWidget(id);
    case OutPortControlOperation:
        return createOutPortControlWidget(id);
    case UpgradeOperation:
        return createUpgradeWidget(id);
    default:
        return createPlainResultWidget(id);
    }
}

QWidget *yunxingcanshu::createPlainResultWidget(OperationId id)
{
    QWidget *container = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(8, 6, 0, 6);
    layout->setSpacing(0);

    QLabel *resultLabel = createResultLabel();
    layout->addWidget(resultLabel);
    m_rows[id].resultLabel = resultLabel;
    return container;
}

QWidget *yunxingcanshu::createInputPortConfigWidget(OperationId id)
{
    QWidget *controls = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(controls);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    QComboBox *portCombo = new QComboBox;
    portCombo->addItems({QStringLiteral("S1"), QStringLiteral("S2")});
    portCombo->setMinimumWidth(96);
    portCombo->setMinimumHeight(30);

    QComboBox *modeCombo = new QComboBox;
    modeCombo->addItems({QStringLiteral("雨量采集"), QStringLiteral("门磁状态")});
    modeCombo->setMinimumWidth(180);
    modeCombo->setMinimumHeight(30);

    layout->addWidget(portCombo);
    layout->addWidget(modeCombo);
    layout->addStretch();

    QLabel *resultLabel = createResultLabel();
    m_rows[id].comboA = portCombo;
    m_rows[id].comboB = modeCombo;
    m_rows[id].resultLabel = resultLabel;
    return wrapControlWithResult(controls, resultLabel);
}

QWidget *yunxingcanshu::createOutPortControlWidget(OperationId id)
{
    QWidget *controls = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(controls);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QComboBox *portCombo = new QComboBox;
    portCombo->addItems({QStringLiteral("SO1"), QStringLiteral("SO2")});
    portCombo->setMinimumWidth(96);
    portCombo->setMinimumHeight(30);

    QComboBox *modeCombo = new QComboBox;
    modeCombo->addItems({QStringLiteral("相机补光灯控制"), QStringLiteral("现场声光报警")});
    modeCombo->setMinimumWidth(180);
    modeCombo->setMinimumHeight(30);

    layout->addWidget(portCombo);
    layout->addWidget(modeCombo);
    layout->addStretch();

    QLabel *resultLabel = createResultLabel();
    m_rows[id].comboA = portCombo;
    m_rows[id].comboB = modeCombo;
    m_rows[id].resultLabel = resultLabel;
    return wrapControlWithResult(controls, resultLabel);
}

QWidget *yunxingcanshu::createUpgradeWidget(OperationId id)
{
    QWidget *controls = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(controls);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QLineEdit *fileEdit = new QLineEdit;
    fileEdit->setReadOnly(true);
    fileEdit->setPlaceholderText(QStringLiteral("请选择升级文件"));
    fileEdit->setMinimumWidth(260);
    fileEdit->setMinimumHeight(32);

    QPushButton *browseButton = new QPushButton(QStringLiteral("选择文件"));
    connect(browseButton, &QPushButton::clicked, this, &yunxingcanshu::chooseUpgradeFile);

    QPushButton *cancelButton = new QPushButton(QStringLiteral("取消升级"));
    cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#fef3f2;color:#b42318;border:1px solid #fecdca;border-radius:8px;padding:6px 12px;font-weight:600;}"
        "QPushButton:hover{background:#fee4e2;}"
        "QPushButton:pressed{background:#fecdca;}"));
    connect(cancelButton, &QPushButton::clicked, this, &yunxingcanshu::cancelUpgradeTransfer);

    layout->addWidget(fileEdit, 1);
    layout->addWidget(browseButton);
    layout->addWidget(cancelButton);

    QLabel *resultLabel = createResultLabel();
    m_rows[id].lineEdit = fileEdit;
    m_rows[id].auxButton = browseButton;
    m_rows[id].cancelButton = cancelButton;
    m_rows[id].resultLabel = resultLabel;
    return wrapControlWithResult(controls, resultLabel);
}

void yunxingcanshu::setResult(OperationId id, const QString &text)
{
    if (m_rows.contains(id) && m_rows[id].resultLabel != nullptr) {
        m_rows[id].resultLabel->setText(text);
    }
}

QByteArray yunxingcanshu::currentPayload() const
{
    const uint16_t datelen =
        (static_cast<uint16_t>(m_RTU_ParameterSetting->m_recivemessage.messageByte[11]) << 8) |
        static_cast<uint16_t>(m_RTU_ParameterSetting->m_recivemessage.messageByte[12]);
    const uint16_t totalLen = datelen + 15;
    if (totalLen < 23) {
        return {};
    }

    return QByteArray(reinterpret_cast<const char *>(m_RTU_ParameterSetting->m_recivemessage.messageByte + 22),
                      totalLen - 23);
}

QString yunxingcanshu::formatMessageTime() const
{
    const uint8_t *timeData = m_RTU_ParameterSetting->m_recivemessage.message.Messagetime;
    return QStringLiteral("20%1-%2-%3 %4:%5:%6")
        .arg(BCD2Hex(timeData[0]), 2, 10, QLatin1Char('0'))
        .arg(BCD2Hex(timeData[1]), 2, 10, QLatin1Char('0'))
        .arg(BCD2Hex(timeData[2]), 2, 10, QLatin1Char('0'))
        .arg(BCD2Hex(timeData[3]), 2, 10, QLatin1Char('0'))
        .arg(BCD2Hex(timeData[4]), 2, 10, QLatin1Char('0'))
        .arg(BCD2Hex(timeData[5]), 2, 10, QLatin1Char('0'));
}

QString yunxingcanshu::extractPrintableText(const QByteArray &payload) const
{
    QString longest;
    QString current;
    for (char ch : payload) {
        const uchar value = static_cast<uchar>(ch);
        if (value >= 32 && value <= 126) {
            current.append(QChar(value));
        } else {
            if (current.size() > longest.size()) {
                longest = current;
            }
            current.clear();
        }
    }

    if (current.size() > longest.size()) {
        longest = current;
    }
    return longest.trimmed();
}

void yunxingcanshu::sendFrame(uint8_t afn, const QByteArray &payload, int8_t sFlag)
{
    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_Device_connection == nullptr) {
        return;
    }

    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage), afn, sFlag,
                           static_cast<int16_t>(payload.size() + 8), MTYPE_DOWN);
    pos += 22;
    if (!payload.isEmpty()) {
        memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte + pos, payload.constData(), payload.size());
        pos += payload.size();
    }
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    const uint16_t crcValue = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte, pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos + 1] = (crcValue & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crcValue >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}

bool yunxingcanshu::encodeUpgradeFrame(uint8_t *frame,
                                       uint32_t &length,
                                       int packetIndex,
                                       const QByteArray &packetData)
{
    if (frame == nullptr ||
        packetIndex <= 0 ||
        m_upgradeTotalPackets <= 0 ||
        packetIndex > m_upgradeTotalPackets ||
        packetData.isEmpty()) {
        return false;
    }

    const uint16_t totalPackets = static_cast<uint16_t>(m_upgradeTotalPackets);
    const uint16_t packetPos = static_cast<uint16_t>(packetIndex);
    const bool hasFrameInfo =
        m_RTU_ParameterSetting->FrameBaseInfo.centeraddr != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[0] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[1] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[2] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[3] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[4] != 0;
    const uint8_t defaultTaddr[5] = {0x00, 0x07, 0x56, 0x46, 0x30};

    uint32_t pos = 0;
    frame[pos++] = 0x7E;
    frame[pos++] = 0x7E;
    memcpy(frame + pos,
           hasFrameInfo ? m_RTU_ParameterSetting->FrameBaseInfo.testaddr : defaultTaddr,
           5);
    pos += 5;
    frame[pos++] = hasFrameInfo ? m_RTU_ParameterSetting->FrameBaseInfo.centeraddr : 0x10;

    const uint16_t pwd = hasFrameInfo ? m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword : 0x0101;
    frame[pos++] = static_cast<uint8_t>((pwd >> 8) & 0xff);
    frame[pos++] = static_cast<uint8_t>(pwd & 0xff);

    frame[pos++] = AFN_UPGRADERTU;
    const uint16_t payloadLen = static_cast<uint16_t>(8 + 3 + packetData.size());
    frame[pos++] = static_cast<uint8_t>(0x80 | (static_cast<uint8_t>((payloadLen >> 8) & 0x0f)));
    frame[pos++] = static_cast<uint8_t>(payloadLen & 0xff);
    frame[pos++] = kControlSyn;
    frame[pos++] = makePackedByte(static_cast<uint8_t>((totalPackets >> 8) & 0x0f),
                                  static_cast<uint8_t>((totalPackets >> 4) & 0x0f));
    frame[pos++] = makePackedByte(static_cast<uint8_t>(totalPackets & 0x0f),
                                  static_cast<uint8_t>((packetPos >> 8) & 0x0f));
    frame[pos++] = makePackedByte(static_cast<uint8_t>((packetPos >> 4) & 0x0f),
                                  static_cast<uint8_t>(packetPos & 0x0f));
    frame[pos++] = static_cast<uint8_t>((m_upgradeSerial >> 8) & 0xff);
    frame[pos++] = static_cast<uint8_t>(m_upgradeSerial & 0xff);

    time_t t = time(nullptr);
    struct tm *info = localtime(&t);
    frame[pos++] = Hex2BCD((info->tm_year + 1900) % 100);
    frame[pos++] = Hex2BCD(info->tm_mon + 1);
    frame[pos++] = Hex2BCD(info->tm_mday);
    frame[pos++] = Hex2BCD(info->tm_hour);
    frame[pos++] = Hex2BCD(info->tm_min);
    frame[pos++] = Hex2BCD(info->tm_sec);

    memcpy(frame + pos, packetData.constData(), packetData.size());
    pos += static_cast<uint32_t>(packetData.size());
    frame[pos++] = ENQ;

    const uint16_t crcValue = cal_crc16(frame, pos);
    frame[pos++] = static_cast<uint8_t>((crcValue >> 8) & 0xff);
    frame[pos++] = static_cast<uint8_t>(crcValue & 0xff);
    length = pos;
    return true;
}

void yunxingcanshu::sendUpgradePacket(int packetIndex)
{
    if (m_RTU_ParameterSetting == nullptr ||
        m_RTU_ParameterSetting->m_Device_connection == nullptr ||
        packetIndex <= 0 ||
        packetIndex > m_upgradeTotalPackets) {
        return;
    }

    const int offset = (packetIndex - 1) * kUpgradePacketLen;
    const QByteArray packetData = m_upgradeFileData.mid(offset, kUpgradePacketLen);
    if (packetData.isEmpty()) {
        setResult(UpgradeOperation, QStringLiteral("升级包数据为空，已停止"));
        resetUpgradeState();
        return;
    }

    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    uint32_t frameLength = 0;
    if (!encodeUpgradeFrame(m_RTU_ParameterSetting->m_sendmessage.messageByte,
                            frameLength,
                            packetIndex,
                            packetData)) {
        m_RTU_ParameterSetting->sendmessager_lock.unlock();
        setResult(UpgradeOperation, QStringLiteral("Upgrade frame encode failed"));
        resetUpgradeState();
        return;
    }
    m_RTU_ParameterSetting->sMessageLen = frameLength;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();

    m_upgradeCurrentPacket = packetIndex;
    setResult(UpgradeOperation,
              QStringLiteral("正在发送升级包 %1/%2")
                  .arg(packetIndex)
                  .arg(m_upgradeTotalPackets));
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}

void yunxingcanshu::beginUpgradeTransfer()
{
    QFile file(m_upgradeFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setResult(UpgradeOperation, QStringLiteral("升级文件打开失败"));
        return;
    }

    m_upgradeFileData = file.readAll();
    file.close();

    if (m_upgradeFileData.isEmpty()) {
        setResult(UpgradeOperation, QStringLiteral("升级文件为空"));
        resetUpgradeState();
        return;
    }

    m_upgradeTotalPackets = (m_upgradeFileData.size() + kUpgradePacketLen - 1) / kUpgradePacketLen;
    m_upgradeCurrentPacket = 0;
    m_upgradeSerial = nextUpgradeSerial();
    m_upgradeCancelled = false;
    m_upgradeInProgress = true;
    sendUpgradePacket(1);
}

void yunxingcanshu::cancelUpgradeTransfer()
{
    if (!m_upgradeInProgress) {
        setResult(UpgradeOperation, QStringLiteral("当前没有正在进行的远程升级"));
        return;
    }

    m_upgradeCancelled = true;
    resetUpgradeState(false);
    setResult(UpgradeOperation, QStringLiteral("远程升级已取消，不再继续发送后续分包"));
}

void yunxingcanshu::resetUpgradeState(bool clearCancelled)
{
    m_upgradeFileData.clear();
    m_upgradeTotalPackets = 0;
    m_upgradeCurrentPacket = 0;
    m_upgradeSerial = 0;
    m_upgradeInProgress = false;
    if (clearCancelled) {
        m_upgradeCancelled = false;
    }
}

void yunxingcanshu::triggerOperation(OperationId id)
{
    switch (id) {
    case QueryTimeOperation:
        setResult(id, QStringLiteral("等待测站返回时间"));
        sendFrame(AFN_QUERYTIME);
        break;
    case AdjustTimeOperation:
        setResult(id, QStringLiteral("按当前电脑时间校准"));
        sendFrame(AFN_ADJUSTTIME);
        break;
    case QueryVersionOperation:
        setResult(id, QStringLiteral("等待版本返回"));
        sendFrame(AFN_QUERYVERSION);
        break;
    case QueryStatusOperation:
        setResult(id, QStringLiteral("等待工况返回"));
        sendFrame(AFN_QUERYSTATUS);
        break;
    case InputPortConfigOperation:
        setResult(id, QStringLiteral("--"));
        break;
    case InputPortQueryOperation:
        setResult(id, QStringLiteral("--"));
        break;
    case OutPortControlOperation: {
        const int portId = m_rows[id].comboA ? (m_rows[id].comboA->currentIndex() + 1) : 1;
        const int controlMethod = m_rows[id].comboB ? m_rows[id].comboB->currentIndex() : 0;
        const int minutes = 0;

        QByteArray payload;
        payload.append(static_cast<char>(portId));
        payload.append(static_cast<char>(controlMethod));
        payload.append(static_cast<char>((minutes >> 8) & 0xff));
        payload.append(static_cast<char>(minutes & 0xff));

        setResult(id, QStringLiteral("已发送: %1 / %2")
                          .arg(m_rows[id].comboA ? m_rows[id].comboA->currentText() : QStringLiteral("SO1"))
                          .arg(m_rows[id].comboB ? m_rows[id].comboB->currentText() : QStringLiteral("相机补光灯控制")));
        sendFrame(AFN_OUTPORTCONTROL, payload);
        break;
    }
    case OutPortQueryOperation:
        setResult(id, QStringLiteral("等待输出端口状态返回"));
        sendFrame(AFN_OUTPORTQUERY);
        break;
    case RestartStationOperation:
        setResult(id, QStringLiteral("远程重启命令已发送"));
        sendFrame(AFN_RESETRTU);
        break;
    case RestoreFactoryOperation: {
        setResult(id, QStringLiteral("恢复出厂设置命令已发送"));
        QByteArray payload;
        payload.append(static_cast<char>(0x98));
        payload.append(static_cast<char>(0x98));
        sendFrame(AFN_RESETFACTORY, payload);
        break;
    }
    case FormatSdOperation: {
        setResult(id, QStringLiteral("格式化SD卡命令已发送"));
        QByteArray payload;
        payload.append(static_cast<char>(0x97));
        payload.append(static_cast<char>(0x97));
        sendFrame(AFN_FORMATSD, payload);
        break;
    }
    case UpgradeOperation:
        if (m_upgradeInProgress) {
            setResult(id, QStringLiteral("升级进行中，请等待当前任务完成"));
        } else if (m_upgradeFilePath.isEmpty()) {
            setResult(id, QStringLiteral("请先选择升级文件"));
        } else {
            beginUpgradeTransfer();
        }
        break;
    }
}

void yunxingcanshu::chooseUpgradeFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择升级文件"));
    if (path.isEmpty()) {
        return;
    }

    m_upgradeFilePath = path;
    if (m_rows.contains(UpgradeOperation) && m_rows[UpgradeOperation].lineEdit != nullptr) {
        m_rows[UpgradeOperation].lineEdit->setText(path);
    }
    setResult(UpgradeOperation, QStringLiteral("已选择升级文件"));
}

void yunxingcanshu::handleQueryTimeResponse()
{
    setResult(QueryTimeOperation, formatMessageTime());
}

void yunxingcanshu::handleAdjustTimeResponse()
{
    setResult(AdjustTimeOperation, QStringLiteral("测站时间已校准为 %1").arg(formatMessageTime()));
}

void yunxingcanshu::handleVersionResponse()
{
    const QByteArray payload = currentPayload();
    QString version = extractPrintableText(payload);
    if (version.isEmpty()) {
        version = payload.toHex().toUpper();
    }
    setResult(QueryVersionOperation, version);
}

void yunxingcanshu::handleStatusResponse()
{
    const QByteArray payload = currentPayload();
    if (payload.size() < 4) {
        setResult(QueryStatusOperation, QStringLiteral("工况报文过短: %1").arg(QString::fromLatin1(payload.toHex().toUpper())));
        return;
    }

    const uint8_t byte2 = static_cast<uint8_t>(payload.at(2));
    const uint8_t byte3 = static_cast<uint8_t>(payload.at(3));

    const bool acPowerNormal = (byte2 & (1 << 4)) == 0;
    const bool batteryUndervoltage = (byte2 & (1 << 2)) != 0;
    const bool chargeNormal = (byte2 & (1 << 1)) != 0;
    const bool cabinetDoorOpen = (byte3 & (1 << 7)) != 0;

    const QString summary = QStringLiteral("交流电%1, 电池欠压%2, 充电状态%3, 箱门%4")
                                .arg(yesNoText(acPowerNormal))
                                .arg(batteryUndervoltage ? QStringLiteral("告警") : QStringLiteral("正常"))
                                .arg(chargeNormal ? QStringLiteral("正常") : QStringLiteral("异常"))
                                .arg(cabinetDoorOpen ? QStringLiteral("打开") : QStringLiteral("关闭"));
    setResult(QueryStatusOperation, summary);

    const QString inputSummary = QStringLiteral("S1/S2 当前状态需结合现场定义判断，原始工况: %1")
                                     .arg(QString::fromLatin1(payload.left(4).toHex().toUpper()));
    setResult(InputPortQueryOperation, inputSummary);
}

void yunxingcanshu::handleFormatSdResponse()
{
    setResult(FormatSdOperation, QStringLiteral("SD卡格式化完成"));
}

void yunxingcanshu::handleRestoreFactoryResponse()
{
    setResult(RestoreFactoryOperation, QStringLiteral("恢复出厂设置完成"));
}

void yunxingcanshu::handleRestartResponse()
{
    setResult(RestartStationOperation, QStringLiteral("测站已确认远程重启命令"));
}

void yunxingcanshu::handleOutPortControlResponse()
{
    setResult(OutPortControlOperation, QStringLiteral("输出端口控制已确认"));
}

void yunxingcanshu::handleOutPortQueryResponse()
{
    const QByteArray payload = currentPayload();
    if (payload.isEmpty()) {
        setResult(OutPortQueryOperation, QStringLiteral("输出端口查询返回为空"));
        return;
    }

    const int count = static_cast<uint8_t>(payload.at(0));
    QStringList lines;
    for (int i = 0; i < count; ++i) {
        const int base = 1 + i * 4;
        if (base + 3 >= payload.size()) {
            break;
        }

        const uint8_t portFlag = static_cast<uint8_t>(payload.at(base));
        const uint8_t status = static_cast<uint8_t>(payload.at(base + 1));
        const uint16_t time = static_cast<uint16_t>((static_cast<uint8_t>(payload.at(base + 2)) << 8) |
                                                    static_cast<uint8_t>(payload.at(base + 3)));
        lines.append(QStringLiteral("端口%1: %2, 持续=%3分钟")
                         .arg(portFlag)
                         .arg(portStatusText(status))
                         .arg(time));
    }

    setResult(OutPortQueryOperation,
              lines.isEmpty() ? QString::fromLatin1(payload.toHex().toUpper()) : lines.join('\n'));
}

void yunxingcanshu::handleUpgradeResponse()
{
    const uint8_t afn = m_RTU_ParameterSetting->m_recivemessage.message.AFN;
    const QByteArray payload = currentPayload();

    if (!m_upgradeInProgress || m_upgradeTotalPackets <= 0) {
        if (m_upgradeCancelled) {
            return;
        }
        setResult(UpgradeOperation, QStringLiteral("Upgrade ACK received"));
        return;
    }

    int packetTotal = 0;
    int packetPos = 0;

    if (afn == AFN_UPGRADERTU) {
        const uint8_t *frame = m_RTU_ParameterSetting->m_recivemessage.messageByte;
        const uint8_t packetCtl1 = frame[14];
        const uint8_t packetCtl2 = frame[15];
        const uint8_t packetCtl3 = frame[16];

        packetTotal =
            static_cast<int>(((packetCtl1 >> 4) & 0x0f) << 8) |
            static_cast<int>((packetCtl1 & 0x0f) << 4) |
            static_cast<int>((packetCtl2 >> 4) & 0x0f);
        packetPos =
            static_cast<int>(packetCtl3) |
            static_cast<int>((packetCtl2 & 0x0f) << 8);
    } else if (afn == 0x00 &&
               !payload.isEmpty() &&
               static_cast<uint8_t>(payload.at(0)) == AFN_UPGRADERTU) {
        if (payload.size() >= 2) {
            const uint8_t status = static_cast<uint8_t>(payload.at(1));
            if (status == 0x15) {
                setResult(UpgradeOperation,
                          QStringLiteral("Upgrade NAK: %1")
                              .arg(QString::fromLatin1(payload.toHex(' ').toUpper())));
                m_upgradeCancelled = false;
                resetUpgradeState();
                return;
            }
            if (status != 0x06) {
                setResult(UpgradeOperation,
                          QStringLiteral("Upgrade resp: %1")
                              .arg(QString::fromLatin1(payload.toHex(' ').toUpper())));
                m_upgradeCancelled = false;
                resetUpgradeState();
                return;
            }
        }
        packetTotal = m_upgradeTotalPackets;
        packetPos = m_upgradeCurrentPacket;
    }

    const int effectiveTotal = packetTotal > 0 ? packetTotal : m_upgradeTotalPackets;
    const int effectivePos = packetPos > 0 ? packetPos : m_upgradeCurrentPacket;
    const bool reachLastPacketByReport = packetTotal > 0 && packetPos >= packetTotal;
    const bool reachLastPacketByLocalState =
        effectiveTotal > 0 &&
        m_upgradeCurrentPacket >= effectiveTotal &&
        (packetPos + 1 >= effectiveTotal || packetPos >= m_upgradeCurrentPacket);

    if (reachLastPacketByReport || reachLastPacketByLocalState) {
        setResult(UpgradeOperation, QStringLiteral("Upgrade complete 100%% (%1/%2)")
                                      .arg(effectiveTotal)
                                      .arg(effectiveTotal));
        m_upgradeCancelled = false;
        resetUpgradeState();
        return;
    }

    int progress = static_cast<int>((static_cast<float>(effectivePos) /
                                     static_cast<float>(effectiveTotal > 0 ? effectiveTotal : 1)) * 100.0f);
    if (progress >= 100) {
        progress = 99;
    }

    setResult(UpgradeOperation, QStringLiteral("Upgrade progress %1%%, ack %2/%3")
                                  .arg(progress)
                                  .arg(effectivePos)
                                  .arg(effectiveTotal));
    const int nextPacket = qMax(m_upgradeCurrentPacket, effectivePos) + 1;
    sendUpgradePacket(nextPacket);
}
void yunxingcanshu::on_set_Button_clicked()
{
    triggerOperation(QueryStatusOperation);
}
