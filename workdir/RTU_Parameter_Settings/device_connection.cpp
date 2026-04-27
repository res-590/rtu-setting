#include "device_connection.h"
#include "ui_device_connection.h"
#include "RTU_ParameterSetting.h"
#include "dtuset.h"
#include "sensor.h"
#include "yunxingcanshu.h"
#include "mainwindow.h"

#include <QMessageBox>
#include <QTextCursor>

namespace {
void appendRuntimeLog(Ui::Device_connection *ui, const QString &text)
{
    if (ui == nullptr || ui->textEdit == nullptr || text.isEmpty()) {
        return;
    }

    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->insertPlainText(text);
    ui->textEdit->moveCursor(QTextCursor::End);
}

void appendReceiveLog(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    if (m_RTU_ParameterSetting != nullptr &&
        m_RTU_ParameterSetting->m_MainWindow != nullptr) {
        m_RTU_ParameterSetting->m_MainWindow->appendSideLog(text);
    }
}

QString formatHexWithSpaces(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

QString afnDescription(uint8_t afn)
{
    switch (afn) {
    case AFN_SETRTUPARAM:
        return QStringLiteral("设置 DTU 参数");
    case AFN_SETDTUPARAM:
        return QStringLiteral("设置端口参数");
    case AFN_READRTUPARAM:
        return QStringLiteral("读取 DTU 参数");
    case AFN_READDTUPARAM:
        return QStringLiteral("读取端口参数");
    case AFN_BASEPARAM:
        return QStringLiteral("设置基础参数");
    case AFN_READBASEPARAM:
        return QStringLiteral("读取基础参数");
    case AFN_SETRUNPARAM:
        return QStringLiteral("设置运行参数");
    case AFN_READRUNRPARAM:
        return QStringLiteral("读取运行参数");
    case AFN_QUERYVERSION:
        return QStringLiteral("查询版本");
    case AFN_QUERYSTATUS:
        return QStringLiteral("查询工况");
    case AFN_FORMATSD:
        return QStringLiteral("格式化 SD 卡");
    case AFN_RESETFACTORY:
        return QStringLiteral("恢复出厂设置");
    case AFN_ADJUSTTIME:
        return QStringLiteral("校准测站时间");
    case AFN_QUERYTIME:
        return QStringLiteral("查询测站时间");
    case AFN_RESETRTU:
        return QStringLiteral("远程重启测站");
    case AFN_OUTPORTCONTROL:
        return QStringLiteral("输出端口控制");
    case AFN_OUTPORTQUERY:
        return QStringLiteral("输出端口状态查询");
    case AFN_UPGRADERTU:
        return QStringLiteral("远程升级");
    case 0x2F:
        return QStringLiteral("学习帧");
    case 0x30:
        return QStringLiteral("传感器相关返回");
    case 0x34:
        return QStringLiteral("运行参数异步返回");
    default:
        return QStringLiteral("未知报文");
    }
}

QString afnLabel(uint8_t afn)
{
    return QStringLiteral("AFN 0x%1：%2")
        .arg(afn, 2, 16, QLatin1Char('0'))
        .arg(afnDescription(afn));
}

void learnFrameBaseInfo(RTU_Message_s &message)
{
    m_RTU_ParameterSetting->FrameBaseInfo.centeraddr = message.message.Caddr;
    memcpy(m_RTU_ParameterSetting->FrameBaseInfo.testaddr, message.message.Taddr, 5);
    m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword = message.message.Pwd;
    SwapDateByte(reinterpret_cast<uint8_t *>(&m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword),
                 sizeof(m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword));
}
}

Device_connection::Device_connection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Device_connection)
{
    ui->setupUi(this);
    ui->summaryCard->hide();

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ui->PortBox->addItem(info.portName());
    }

    const int com4Index = ui->PortBox->findText(QStringLiteral("COM4"));
    if (com4Index >= 0) {
        ui->PortBox->setCurrentIndex(com4Index);
    }

    ui->BackButton->hide();

    const int baud57600Index = ui->BaudBox->findText(QStringLiteral("57600"));
    if (baud57600Index >= 0) {
        ui->BaudBox->setCurrentIndex(baud57600Index);
    } else {
        ui->BaudBox->setCurrentIndex(0);
    }

    m_WorkThread = new SerialWorkThread;
    m_WorkThread->serial = nullptr;
    m_WorkThread->hasMessage = false;
    m_WorkThread->serial_status = false;
    m_DeencodeThread = nullptr;

    setWindowTitle(QStringLiteral("通信设置"));
    ui->configTitleLabel->setText(QStringLiteral("串口配置"));
    ui->configHintLabel->setText(QStringLiteral("确认串口、波特率和帧格式后再打开连接，连接成功后配置项会锁定。"));
    ui->serialStatusTitleLabel->setText(QStringLiteral("连接状态"));
    ui->serialGroupTitleLabel->setText(QStringLiteral("基础连接"));
    ui->serialGroupTitleLabel_2->setText(QStringLiteral("帧格式"));
    ui->monitorTitleLabel->setText(QStringLiteral("通信控制台"));
    ui->monitorPortLabel->setWordWrap(true);
    ui->sendHintLabel->setText(QStringLiteral("输入调试指令后发送。"));
    ui->OpenSerialButton->setText(QStringLiteral("打开串口"));
    ui->BackButton->setText(QStringLiteral("关闭页面"));
    ui->SendButton->setText(QStringLiteral("发送"));
    ui->label->setText(QStringLiteral("串口"));
    ui->label_2->setText(QStringLiteral("波特率"));
    ui->label_3->setText(QStringLiteral("数据位"));
    ui->label_4->setText(QStringLiteral("校验位"));
    ui->label_6->setText(QStringLiteral("停止位"));
    ui->label_7->setText(QStringLiteral("流控"));
    ui->logTitleLabel->setText(QStringLiteral("运行日志"));
    ui->sendTitleLabel->setText(QStringLiteral("发送内容"));
    if (ui->textEdit != nullptr) {
        ui->textEdit->setPlainText(QStringLiteral("串口打开、关闭、发送和异常信息会显示在这里。\r\n"));
    }
    ui->OpenSerialButton->setMinimumHeight(40);
    ui->BackButton->setMinimumHeight(40);
    ui->SendButton->setMinimumHeight(42);

    updateConnectionPanel(false);
}

Device_connection::~Device_connection()
{
    closeSerial();
    delete m_WorkThread;
    delete ui;
}

void Device_connection::sendmessage()
{
    if (m_RTU_ParameterSetting->m_Device_connection == nullptr ||
        m_WorkThread == nullptr ||
        !m_WorkThread->serial_status ||
        m_WorkThread->serial == nullptr) {
        QMessageBox::about(this, QStringLiteral("错误"), QStringLiteral("串口未打开"));
        return;
    }

    uint16_t sendlength = 0;
    uint8_t *sendbuffer = nullptr;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    sendlength = m_RTU_ParameterSetting->sMessageLen;
    sendbuffer = static_cast<uint8_t *>(malloc(sendlength));
    memcpy(sendbuffer, m_RTU_ParameterSetting->m_sendmessage.messageByte, sendlength);
    m_RTU_ParameterSetting->sendmessager_lock.unlock();

    const QByteArray sendBytes(reinterpret_cast<const char *>(sendbuffer), sendlength);
    appendRuntimeLog(ui, QStringLiteral("发送报文（%1 字节，%2）\r\n")
                            .arg(sendlength)
                            .arg(afnLabel(m_RTU_ParameterSetting->m_sendmessage.message.AFN)));
    appendRuntimeLog(ui, formatHexWithSpaces(sendBytes) + QStringLiteral("\r\n"));

    const qint64 written = m_WorkThread->serial->write(reinterpret_cast<char *>(sendbuffer), sendlength);
    if (written != sendlength) {
        appendRuntimeLog(ui, QStringLiteral("发送警告：期望 %1 字节，实际 %2 字节，错误：%3\r\n")
                                .arg(sendlength)
                                .arg(written)
                                .arg(m_WorkThread->serial->errorString()));
    }
    free(sendbuffer);
}

bool Device_connection::isSerialOpen() const
{
    return m_WorkThread != nullptr && m_WorkThread->serial_status;
}

void Device_connection::updateConnectionPanel(bool connected)
{
    if (!ui) {
        return;
    }

    ui->serialStatusValueLabel->setText(connected ? tr("已连接") : tr("未连接"));
    ui->serialStatusValueLabel->setStyleSheet(connected
                                              ? QStringLiteral("background:#e9f4ff;color:#0f5ea8;border:1px solid #bcd7f1;border-radius:10px;padding:4px 10px;font-weight:700;")
                                              : QStringLiteral("background:#f3f6f9;color:#60717f;border:1px solid #d5dee8;border-radius:10px;padding:4px 10px;font-weight:700;"));
    ui->monitorStatusValueLabel->setText(connected ? tr("串口在线") : tr("等待连接"));
    ui->monitorStatusValueLabel->setStyleSheet(connected
                                               ? QStringLiteral("background:#e9f4ff;color:#0f5ea8;border:1px solid #bcd7f1;border-radius:10px;padding:4px 10px;font-weight:700;")
                                               : QStringLiteral("background:#f3f6f9;color:#60717f;border:1px solid #d5dee8;border-radius:10px;padding:4px 10px;font-weight:700;"));
    ui->monitorPortLabel->setText(tr("串口：%1 / %2")
                                      .arg(ui->PortBox->currentText().isEmpty() ? tr("未选择") : ui->PortBox->currentText())
                                      .arg(ui->BaudBox->currentText().isEmpty() ? tr("--") : ui->BaudBox->currentText()));
}

void SerialWorkThread::run()
{
    while (!isInterruptionRequested()) {
        if (!serial_status || serial == nullptr) {
            msleep(20);
            continue;
        }

        if (hasMessage && !serial->waitForReadyRead(100)) {
            hasMessage = false;
            emit m_RTU_ParameterSetting->m_Device_connection->get_Onemessage();
        } else if (!hasMessage) {
            msleep(20);
        }
    }
}

void Device_connection::message_handle()
{
    processReceiveBuffer();
}

void Device_connection::dispatchCurrentMessage()
{
    const uint8_t afn = m_RTU_ParameterSetting->m_recivemessage.message.AFN;
    appendReceiveLog(QStringLiteral("接收完成：%1 字节\r\n").arg(m_RTU_ParameterSetting->rMessageLen));
    appendReceiveLog(QStringLiteral("报文类型：%1\r\n\r\n").arg(afnLabel(afn)));

    if (m_RTU_ParameterSetting->rMessageLen >= 10 &&
        m_RTU_ParameterSetting->m_recivemessage.message.Frame_heder == 0x7E7E) {
        learnFrameBaseInfo(m_RTU_ParameterSetting->m_recivemessage);
    }

    if (afn == AFN_READRTUPARAM) {
        m_RTU_ParameterSetting->m_dtuset->handle_DTUINFO();
    } else if (afn == AFN_READDTUPARAM) {
        m_RTU_ParameterSetting->m_portset->handle_RTUINFO();
    } else if (afn == AFN_READBASEPARAM) {
        if (m_RTU_ParameterSetting->isbasedate && m_RTU_ParameterSetting->m_basicPage != nullptr) {
            m_RTU_ParameterSetting->m_basicPage->handle_baseConfigParam();
        } else if (!m_RTU_ParameterSetting->isbasedate && m_RTU_ParameterSetting->m_sensorPage != nullptr) {
            m_RTU_ParameterSetting->m_sensorPage->handleSensorInfo();
        }
    } else if (afn == AFN_READRUNRPARAM) {
        if (m_RTU_ParameterSetting->isbasedate && m_RTU_ParameterSetting->m_basicPage != nullptr) {
            m_RTU_ParameterSetting->m_basicPage->handle_runtimeConfigParam();
        }
    } else if (afn == AFN_QUERYTIME) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleQueryTimeResponse();
        }
    } else if (afn == AFN_ADJUSTTIME) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleAdjustTimeResponse();
        }
    } else if (afn == AFN_QUERYVERSION) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleVersionResponse();
        }
    } else if (afn == AFN_QUERYSTATUS) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleStatusResponse();
        }
    } else if (afn == AFN_FORMATSD) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleFormatSdResponse();
        }
    } else if (afn == AFN_RESETFACTORY) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleRestoreFactoryResponse();
        }
    } else if (afn == AFN_RESETRTU) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleRestartResponse();
        }
    } else if (afn == AFN_OUTPORTCONTROL) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleOutPortControlResponse();
        }
    } else if (afn == AFN_OUTPORTQUERY) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleOutPortQueryResponse();
        }
    } else if (afn == AFN_UPGRADERTU) {
        if (m_RTU_ParameterSetting->m_runtimePage != nullptr) {
            m_RTU_ParameterSetting->m_runtimePage->handleUpgradeResponse();
        }
    } else if (afn == 0x2F) {
        if (m_RTU_ParameterSetting->isbasedate &&
            !m_RTU_ParameterSetting->baseReadRetriedAfterLearn &&
            m_RTU_ParameterSetting->m_basicPage != nullptr) {
            m_RTU_ParameterSetting->baseReadRetriedAfterLearn = true;
            appendRuntimeLog(ui, QStringLiteral("已根据学习帧自动重试基础参数读取\r\n"));
            m_RTU_ParameterSetting->m_basicPage->requestBaseParamRead();
        } else {
            appendRuntimeLog(ui, QStringLiteral("未处理报文：%1\r\n").arg(afnLabel(afn)));
        }
    } else {
        appendRuntimeLog(ui, QStringLiteral("未处理报文：%1\r\n").arg(afnLabel(afn)));
    }

    memset(m_RTU_ParameterSetting->m_recivemessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_recivemessage.messageByte));
}

void Device_connection::processReceiveBuffer()
{
    while (true) {
        const int startIndex = m_receiveBuffer.indexOf(QByteArray::fromHex("7E7E"));
        if (startIndex < 0) {
            m_receiveBuffer.clear();
            return;
        }

        if (startIndex > 0) {
            m_receiveBuffer.remove(0, startIndex);
        }

        if (m_receiveBuffer.size() < 13) {
            return;
        }

        const uint16_t plen =
            (static_cast<uint16_t>(static_cast<uint8_t>(m_receiveBuffer.at(11))) << 8) |
            static_cast<uint16_t>(static_cast<uint8_t>(m_receiveBuffer.at(12)));
        const int frameLength = static_cast<int>(plen) + 17;
        if (m_receiveBuffer.size() < frameLength) {
            return;
        }

        const QByteArray frame = m_receiveBuffer.left(frameLength);
        m_receiveBuffer.remove(0, frameLength);

        memset(m_RTU_ParameterSetting->m_recivemessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_recivemessage.messageByte));
        memcpy(m_RTU_ParameterSetting->m_recivemessage.messageByte, frame.constData(), frame.size());
        m_RTU_ParameterSetting->rMessageLen = static_cast<uint32_t>(frame.size());
        appendReceiveLog(QStringLiteral("接收数据：%1\r\n").arg(formatHexWithSpaces(frame)));
        dispatchCurrentMessage();
    }
}

void Device_connection::on_OpenSerialButton_clicked()
{
    if (ui->OpenSerialButton->text() == tr("打开串口")) {
        if (m_WorkThread->serial != nullptr || m_WorkThread->isRunning()) {
            closeSerial();
        }

        if (ui->PortBox->currentText().isEmpty()) {
            QMessageBox::about(this, QStringLiteral("错误"), QStringLiteral("未选择串口。"));
            return;
        }

        m_WorkThread->serial = new QSerialPort;
        m_WorkThread->serial->setPortName(ui->PortBox->currentText());

        const qint32 baudrate = ui->BaudBox->currentText().toInt();
        switch (baudrate) {
        case 9600:
            m_WorkThread->serial->setBaudRate(QSerialPort::Baud9600);
            break;
        case 19200:
            m_WorkThread->serial->setBaudRate(QSerialPort::Baud19200);
            break;
        case 38400:
            m_WorkThread->serial->setBaudRate(QSerialPort::Baud38400);
            break;
        case 57600:
            m_WorkThread->serial->setBaudRate(QSerialPort::Baud57600);
            break;
        case 115200:
            m_WorkThread->serial->setBaudRate(QSerialPort::Baud115200);
            break;
        default:
            break;
        }

        m_WorkThread->serial->setDataBits(QSerialPort::Data8);
        m_WorkThread->serial->setParity(QSerialPort::NoParity);
        m_WorkThread->serial->setFlowControl(QSerialPort::NoFlowControl);
        m_WorkThread->serial->setStopBits(QSerialPort::OneStop);

        if (!m_WorkThread->serial->open(QIODevice::ReadWrite)) {
            const QString errorText = m_WorkThread->serial->errorString();
            QMessageBox::about(this, QStringLiteral("错误"),
                               QStringLiteral("串口打开失败：%1").arg(errorText));
            appendRuntimeLog(ui, QStringLiteral("串口打开失败：串口=%1，波特率=%2，错误=%3\r\n")
                                    .arg(ui->PortBox->currentText())
                                    .arg(ui->BaudBox->currentText())
                                    .arg(errorText));
            delete m_WorkThread->serial;
            m_WorkThread->serial = nullptr;
            updateConnectionPanel(false);
            return;
        }

        m_WorkThread->hasMessage = false;
        m_WorkThread->serial_status = true;
        ui->PortBox->setEnabled(false);
        ui->BaudBox->setEnabled(false);
        ui->BitBox->setEnabled(false);
        ui->ParityBox->setEnabled(false);
        ui->StopBox->setEnabled(false);
        ui->ControlBox->setEnabled(false);
        ui->OpenSerialButton->setText(tr("关闭串口"));
        updateConnectionPanel(true);
        emit connectionStatusChanged(true);
        appendRuntimeLog(ui, QStringLiteral("串口已打开：%1，%2，8N1，无流控\r\n")
                                .arg(ui->PortBox->currentText())
                                .arg(ui->BaudBox->currentText()));

        QObject::connect(m_WorkThread->serial, &QSerialPort::readyRead,
                         this, &Device_connection::ReadData, Qt::UniqueConnection);
        QObject::connect(this, &Device_connection::get_Onemessage,
                         this, &Device_connection::message_handle, Qt::UniqueConnection);

        if (!m_WorkThread->isRunning()) {
            m_WorkThread->start();
        }
    } else {
        closeSerial();
    }
}

void Device_connection::ReadData()
{
    if (m_WorkThread == nullptr || m_WorkThread->serial == nullptr) {
        return;
    }

    const QByteArray info = m_WorkThread->serial->readAll();
    if (info.isEmpty()) {
        return;
    }

    m_WorkThread->hasMessage = true;
    m_receiveBuffer.append(info);
}

void Device_connection::on_SendButton_clicked()
{
    if (!m_WorkThread || !m_WorkThread->serial_status || !m_WorkThread->serial) {
        QMessageBox::about(this, QStringLiteral("错误"), QStringLiteral("串口未打开"));
        return;
    }

    const QByteArray sendbuf = ui->textEdit_2->toPlainText().toUtf8();
    if (sendbuf.isEmpty()) {
        QMessageBox::about(this, QStringLiteral("提示"), QStringLiteral("请输入要发送的内容"));
        return;
    }

    m_WorkThread->serial->write(sendbuf);
    appendRuntimeLog(ui, QStringLiteral("手动发送：%1\r\n").arg(formatHexWithSpaces(sendbuf)));
}

void Device_connection::on_BackButton_clicked()
{
    updateConnectionPanel(isSerialOpen());
    emit connectionStatusChanged(isSerialOpen());
    this->hide();
}

void Device_connection::closeSerial()
{
    if (m_WorkThread == nullptr) {
        return;
    }

    disconnect(this, &Device_connection::get_Onemessage, this, &Device_connection::message_handle);

    m_WorkThread->serial_status = false;
    m_WorkThread->hasMessage = false;

    if (m_WorkThread->serial != nullptr) {
        disconnect(m_WorkThread->serial, &QSerialPort::readyRead, this, &Device_connection::ReadData);
        if (m_WorkThread->serial->isOpen()) {
            m_WorkThread->serial->clear();
            m_WorkThread->serial->close();
        }
        delete m_WorkThread->serial;
        m_WorkThread->serial = nullptr;
    }

    if (m_WorkThread->isRunning()) {
        m_WorkThread->requestInterruption();
        m_WorkThread->wait(500);
    }

    ui->PortBox->setEnabled(true);
    ui->BaudBox->setEnabled(true);
    ui->BitBox->setEnabled(true);
    ui->ParityBox->setEnabled(true);
    ui->StopBox->setEnabled(true);
    ui->ControlBox->setEnabled(true);
    ui->OpenSerialButton->setText(tr("打开串口"));
    updateConnectionPanel(false);
    emit connectionStatusChanged(false);
    appendRuntimeLog(ui, QStringLiteral("串口已关闭\r\n"));
}
