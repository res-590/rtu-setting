#include "portset.h"
#include "ui_portset.h"
#include "RTU_ParameterSetting.h"
#include <QBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>

namespace {
QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:#2f78e8;color:#ffffff;border:none;border-radius:6px;padding:8px 18px;font-weight:600;min-height:38px;}"
        "QPushButton:hover{background:#2468cc;}"
        "QPushButton:pressed{background:#1d57aa;}");
}

QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:#ffffff;color:#2f78e8;border:1px solid #bfd4f6;border-radius:6px;padding:8px 14px;font-weight:600;min-height:38px;}"
        "QPushButton:hover{background:#edf4ff;}"
        "QPushButton:pressed{background:#dbe9ff;}");
}

QStringList portTypeTexts()
{
    return QStringList{
        QStringLiteral("关闭"),
        QStringLiteral("内置DTU"),
        QStringLiteral("外置DTU(RDP)"),
        QStringLiteral("外置DTU(KH)"),
        QStringLiteral("北斗卫星DTU"),
        QStringLiteral("超短波DTU"),
        QStringLiteral("短信DTU"),
        QStringLiteral("外置DTU(透传)"),
        QStringLiteral("RF"),
        QStringLiteral("IC卡"),
        QStringLiteral("RS485传感器"),
        QStringLiteral("RS232传感器"),
        QStringLiteral("格雷码传感器"),
        QStringLiteral("摄像头"),
        QStringLiteral("未知")
    };
}

QVector<int> portTypeCodes()
{
    return QVector<int>{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x10, 0x11, 0x20, 0x21, 0x22, 0x23, 0x12
    };
}
}

portset::portset(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::portset)
{
    ui->setupUi(this);
    ui->summaryCard->hide();
    setWindowTitle(QStringLiteral("端口参数"));
    const auto setLabelText = [this](const char *name, const QString &text) {
        if (QLabel *label = findChild<QLabel *>(QLatin1String(name))) {
            label->setText(text);
            label->setWordWrap(true);
        }
    };
    if (QPushButton *button = findChild<QPushButton *>(QStringLiteral("read_Button"))) {
        button->setText(QStringLiteral("读取参数"));
        button->setStyleSheet(primaryButtonStyle());
        button->setFixedSize(108, 38);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    if (QPushButton *button = findChild<QPushButton *>(QStringLiteral("set_Button"))) {
        button->setText(QStringLiteral("写入参数"));
        button->setStyleSheet(primaryButtonStyle());
        button->setFixedSize(108, 38);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    setLabelText("pageTitle", QStringLiteral("端口参数配置"));
    setLabelText("pageSubtitle", QStringLiteral("按端口维护设备类型、波特率、RF 频点和保留参数。"));
    setLabelText("labelPort1Hint", QStringLiteral("主通信端口"));
    setLabelText("labelPort2Hint", QStringLiteral("扩展端口"));
    setLabelText("labelPort3Hint", QStringLiteral("采集端口"));
    setLabelText("labelPort4Hint", QStringLiteral("外设接口"));
    setLabelText("labelPort5Hint", QStringLiteral("保留 / 兼容端口"));
    const QStringList typeTexts = portTypeTexts();
    for (const QString &name : {QStringLiteral("equit_type1"),
                                QStringLiteral("equit_type2"),
                                QStringLiteral("equit_type3"),
                                QStringLiteral("equit_type4"),
                                QStringLiteral("equit_type5")}) {
        if (QComboBox *combo = findChild<QComboBox *>(name)) {
            combo->clear();
            combo->addItems(typeTexts);
        }
    }
    if (QPushButton *setButton = findChild<QPushButton *>(QStringLiteral("set_Button"))) {
        QPushButton *clearButton = new QPushButton(QStringLiteral("清空内容"), setButton->parentWidget());
        clearButton->setObjectName(QStringLiteral("clearButton"));
        clearButton->setStyleSheet(secondaryButtonStyle());
        clearButton->setFixedSize(108, 38);
        clearButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        if (QBoxLayout *layout = qobject_cast<QBoxLayout *>(setButton->parentWidget()->layout())) {
            if (QPushButton *readButton = findChild<QPushButton *>(QStringLiteral("read_Button"))) {
                layout->insertWidget(layout->indexOf(readButton), clearButton);
            } else {
                layout->insertWidget(layout->indexOf(setButton), clearButton);
            }
        }
        connect(clearButton, &QPushButton::clicked, this, &portset::handleClearButtonClicked);
    }
    if (QFrame *actionCard = findChild<QFrame *>(QStringLiteral("actionCard"))) {
        actionCard->setMinimumWidth(420);
        actionCard->setMaximumWidth(QWIDGETSIZE_MAX);
        actionCard->setFixedHeight(62);
        if (QLayout *layout = actionCard->layout()) {
            layout->setContentsMargins(14, 12, 14, 12);
            layout->setSpacing(12);
        }
    }
    if (QBoxLayout *layout = qobject_cast<QBoxLayout *>(ui->actionCard->layout())) {
        if (QPushButton *button = findChild<QPushButton *>(QStringLiteral("clearButton"))) {
            layout->setAlignment(button, Qt::AlignVCenter);
        }
        if (QPushButton *button = findChild<QPushButton *>(QStringLiteral("read_Button"))) {
            layout->setAlignment(button, Qt::AlignVCenter);
        }
        if (QPushButton *button = findChild<QPushButton *>(QStringLiteral("set_Button"))) {
            layout->setAlignment(button, Qt::AlignVCenter);
        }
    }
    memset(port_info.date,0,sizeof (port_info.date));
}

portset::~portset()
{
    delete ui;
}

uint8_t portset::encodePortTypeIndex(int index) const
{
    const QVector<int> codes = portTypeCodes();
    if (index < 0 || index >= codes.size()) {
        return 0x00;
    }
    return static_cast<uint8_t>(codes.at(index));
}

int portset::decodePortTypeValue(uint8_t value) const
{
    const int code = value;
    const QVector<int> codes = portTypeCodes();
    const int index = codes.indexOf(code);
    if (index >= 0) {
        return index;
    }
    return 0;
}

void portset::syncUiToProtocol()
{
    on_equit_type1_currentIndexChanged(ui->equit_type1->currentIndex());
    on_equit_type2_currentIndexChanged(ui->equit_type2->currentIndex());
    on_equit_type3_currentIndexChanged(ui->equit_type3->currentIndex());
    on_equit_type4_currentIndexChanged(ui->equit_type4->currentIndex());
    on_equit_type5_currentIndexChanged(ui->equit_type5->currentIndex());

    on_baud_rate1_currentIndexChanged(ui->baud_rate1->currentIndex());
    on_baud_rate2_currentIndexChanged(ui->baud_rate2->currentIndex());
    on_baud_rate3_currentIndexChanged(ui->baud_rate3->currentIndex());
    on_baud_rate4_currentIndexChanged(ui->baud_rate4->currentIndex());
    on_baud_rate5_currentIndexChanged(ui->baud_rate5->currentIndex());

    on_RF1_currentIndexChanged(ui->RF1->currentIndex());
    on_RF2_currentIndexChanged(ui->RF2->currentIndex());
    on_RF3_currentIndexChanged(ui->RF3->currentIndex());
    on_RF4_currentIndexChanged(ui->RF4->currentIndex());
    on_RF5_currentIndexChanged(ui->RF5->currentIndex());

    on_keep1_editingFinished();
    on_keep2_editingFinished();
    on_keep3_editingFinished();
    on_keep4_editingFinished();
    on_keep5_editingFinished();
}

void portset::clearDisplayedParameters()
{
    memset(port_info.date, 0, sizeof(port_info.date));

    ui->equit_type1->setCurrentIndex(0);
    ui->equit_type2->setCurrentIndex(0);
    ui->equit_type3->setCurrentIndex(0);
    ui->equit_type4->setCurrentIndex(0);
    ui->equit_type5->setCurrentIndex(0);

    ui->baud_rate1->setCurrentIndex(0);
    ui->baud_rate2->setCurrentIndex(0);
    ui->baud_rate3->setCurrentIndex(0);
    ui->baud_rate4->setCurrentIndex(0);
    ui->baud_rate5->setCurrentIndex(0);

    ui->RF1->setCurrentIndex(0);
    ui->RF2->setCurrentIndex(0);
    ui->RF3->setCurrentIndex(0);
    ui->RF4->setCurrentIndex(0);
    ui->RF5->setCurrentIndex(0);

    ui->keep1->clear();
    ui->keep2->clear();
    ui->keep3->clear();
    ui->keep4->clear();
    ui->keep5->clear();
}

void portset::on_set_Button_clicked()
{
    syncUiToProtocol();
    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte,0,sizeof (m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),AFN_SETDTUPARAM,STX,sizeof(this->port_info.date) + 8,MTYPE_DOWN);
    pos += 22;
    memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte+pos,this->port_info.date,sizeof(this->port_info.date));
    pos += sizeof(this->port_info.date);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    uint16_t crc_value = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte,pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos+1] = (crc_value & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crc_value >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}

void portset::handleClearButtonClicked()
{
    clearDisplayedParameters();
}


void portset::on_equit_type1_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port1_Type = encodePortTypeIndex(index);
}

void portset::on_equit_type2_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port2_Type = encodePortTypeIndex(index);
}

void portset::on_equit_type3_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port3_Type = encodePortTypeIndex(index);
}

void portset::on_equit_type4_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port4_Type = encodePortTypeIndex(index);
}

void portset::on_equit_type5_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port5_Type = encodePortTypeIndex(index);
}

void portset::on_baud_rate1_currentIndexChanged(int index)
{
    switch (index) {
        case 0:{
        }break;
        case 1:{        ///<300
            uint32_t buad = 300;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 2:{        ///<600
            uint32_t buad = 600;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 3:{        ///<1200
            uint32_t buad = 1200;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 4:{        ///<2400
            uint32_t buad = 2400;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 5:{        ///<4800
            uint32_t buad = 4800;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 6:{        ///<9600
            uint32_t buad = 9600;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 7:{        ///<19200
            uint32_t buad = 19200;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 8:{        ///<38400
            uint32_t buad = 38400;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 9:{        ///<57600
            uint32_t buad = 57600;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        case 10:{       ///<115200
            uint32_t buad = 115200;
            this->port_info.portinfo.Port1_Buand = buad;
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
    SwapDateByte((uint8_t*) &(this->port_info.portinfo.Port1_Buand),sizeof(this->port_info.portinfo.Port1_Buand));
}

void portset::on_baud_rate2_currentIndexChanged(int index)
{
    switch (index) {
        case 0:{
        }break;
        case 1:{        ///<300
            uint32_t buad = 300;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 2:{        ///<600
            uint32_t buad = 600;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 3:{        ///<1200
            uint32_t buad = 1200;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 4:{        ///<2400
            uint32_t buad = 2400;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 5:{        ///<4800
            uint32_t buad = 4800;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 6:{        ///<9600
            uint32_t buad = 9600;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 7:{        ///<19200
            uint32_t buad = 19200;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 8:{        ///<38400
            uint32_t buad = 38400;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 9:{        ///<57600
            uint32_t buad = 57600;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        case 10:{       ///<115200
            uint32_t buad = 115200;
            this->port_info.portinfo.Port2_Buand = buad;
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
    SwapDateByte((uint8_t*) &(this->port_info.portinfo.Port2_Buand),sizeof(this->port_info.portinfo.Port2_Buand));
}

void portset::on_baud_rate3_currentIndexChanged(int index)
{
    switch (index) {
        case 0:{
        }break;
        case 1:{        ///<300
            uint32_t buad = 300;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 2:{        ///<600
            uint32_t buad = 600;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 3:{        ///<1200
            uint32_t buad = 1200;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 4:{        ///<2400
            uint32_t buad = 2400;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 5:{        ///<4800
            uint32_t buad = 4800;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 6:{        ///<9600
            uint32_t buad = 9600;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 7:{        ///<19200
            uint32_t buad = 19200;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 8:{        ///<38400
            uint32_t buad = 38400;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 9:{        ///<57600
            uint32_t buad = 57600;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        case 10:{       ///<115200
            uint32_t buad = 115200;
            this->port_info.portinfo.Port3_Buand = buad;
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
    SwapDateByte((uint8_t*) &(this->port_info.portinfo.Port3_Buand),sizeof(this->port_info.portinfo.Port3_Buand));
}

void portset::on_baud_rate4_currentIndexChanged(int index)
{
    switch (index) {
        case 0:{
        }break;
        case 1:{        ///<300
            uint32_t buad = 300;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 2:{        ///<600
            uint32_t buad = 600;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 3:{        ///<1200
            uint32_t buad = 1200;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 4:{        ///<2400
            uint32_t buad = 2400;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 5:{        ///<4800
            uint32_t buad = 4800;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 6:{        ///<9600
            uint32_t buad = 9600;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 7:{        ///<19200
            uint32_t buad = 19200;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 8:{        ///<38400
            uint32_t buad = 38400;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 9:{        ///<57600
            uint32_t buad = 57600;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        case 10:{       ///<115200
            uint32_t buad = 115200;
            this->port_info.portinfo.Port4_Buand = buad;
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
    SwapDateByte((uint8_t*) &(this->port_info.portinfo.Port4_Buand),sizeof(this->port_info.portinfo.Port4_Buand));
}

void portset::on_baud_rate5_currentIndexChanged(int index)
{
    switch (index) {
        case 0:{
        }break;
        case 1:{        ///<300
            uint32_t buad = 300;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 2:{        ///<600
            uint32_t buad = 600;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 3:{        ///<1200
            uint32_t buad = 1200;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 4:{        ///<2400
            uint32_t buad = 2400;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 5:{        ///<4800
            uint32_t buad = 4800;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 6:{        ///<9600
            uint32_t buad = 9600;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 7:{        ///<19200
            uint32_t buad = 19200;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 8:{        ///<38400
            uint32_t buad = 38400;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 9:{        ///<57600
            uint32_t buad = 57600;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        case 10:{       ///<115200
            uint32_t buad = 115200;
            this->port_info.portinfo.Port5_Buand = buad;
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
    SwapDateByte((uint8_t*) &(this->port_info.portinfo.Port5_Buand),sizeof(this->port_info.portinfo.Port5_Buand));
}

void portset::on_RF1_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port1_RF = index;
}

void portset::on_RF2_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port2_RF = index;
}

void portset::on_RF3_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port3_RF = index;
}

void portset::on_RF4_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port4_RF = index;
}

void portset::on_RF5_currentIndexChanged(int index)
{
    this->port_info.portinfo.Port5_RF = index;
}

void portset::on_keep1_editingFinished()
{
    this->port_info.portinfo.Port1_Reserve = ui->keep1->text().toUInt();
}

void portset::on_keep2_editingFinished()
{
    this->port_info.portinfo.Port2_Reserve = ui->keep2->text().toUInt();
}

void portset::on_keep3_editingFinished()
{
    this->port_info.portinfo.Port3_Reserve = ui->keep3->text().toUInt();
}

void portset::on_keep4_editingFinished()
{
    this->port_info.portinfo.Port4_Reserve = ui->keep4->text().toUInt();
}

void portset::on_keep5_editingFinished()
{
    this->port_info.portinfo.Port5_Reserve = ui->keep5->text().toUInt();
}


void portset::on_read_Button_clicked()
{
    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte,0,sizeof (m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),AFN_READDTUPARAM,STX,8,MTYPE_DOWN);
    pos += 22;
    //memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte+pos,this->dtuinfo.date,sizeof(this->dtuinfo.DTU_info));
    //pos += sizeof(this->dtuinfo.DTU_info);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    uint16_t crc_value = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte,pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos+1] = (crc_value & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crc_value >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}
void portset::fill_portbuand(Port_Protocol &dtuinfobuffer,int comboxindex)
{
    ///<波特率
    QComboBox *CurrentBaud;
    uint32_t *buand;
    switch (comboxindex) {
        case 1:{
            buand = &dtuinfobuffer.portinfo.Port1_Buand;
            CurrentBaud = ui->baud_rate1;
        }break;
        case 2:{
            buand = &dtuinfobuffer.portinfo.Port2_Buand;
            CurrentBaud = ui->baud_rate2;
        }break;
        case 3:{
            buand = &dtuinfobuffer.portinfo.Port3_Buand;
            CurrentBaud = ui->baud_rate3;
        }break;
        case 4:{
            buand = &dtuinfobuffer.portinfo.Port4_Buand;
            CurrentBaud = ui->baud_rate4;
        }break;
        case 5:{
            buand = &dtuinfobuffer.portinfo.Port5_Buand;
            CurrentBaud = ui->baud_rate5;
        }break;
        default:
            qDebug()<<"CurrentBaud  failed!\r\n";
            return;
    }
    SwapDateByte((uint8_t*) buand,sizeof(uint32_t));
    switch (*buand) {
        case 0:{
            CurrentBaud->setCurrentIndex(0);
        }break;
        case 300:{
            CurrentBaud->setCurrentIndex(1);
        }break;
        case 600:{
            CurrentBaud->setCurrentIndex(2);
        }break;
        case 1200:{
            CurrentBaud->setCurrentIndex(3);
        }break;
        case 2400:{
            CurrentBaud->setCurrentIndex(4);
        }break;
        case 4800:{
            CurrentBaud->setCurrentIndex(5);
        }break;
        case 9600:{
            CurrentBaud->setCurrentIndex(6);
        }break;
        case 19200:{
            CurrentBaud->setCurrentIndex(7);
        }break;
        case 38400:{
            CurrentBaud->setCurrentIndex(8);
        }break;
        case 57600:{
            CurrentBaud->setCurrentIndex(9);
        }break;
        case 115200:{
            CurrentBaud->setCurrentIndex(10);
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
}
void portset::handle_RTUINFO(void)
{
    Port_Protocol dtuinfobuffer;
    m_RTU_ParameterSetting->recivemessage_lock.lock();
    QString buffer;
    memcpy(dtuinfobuffer.date,m_RTU_ParameterSetting->m_recivemessage.messageByte + 22,sizeof (dtuinfobuffer.date));
    ///<端口类型
    ui->equit_type1->setCurrentIndex(decodePortTypeValue(dtuinfobuffer.portinfo.Port1_Type));
    ui->equit_type2->setCurrentIndex(decodePortTypeValue(dtuinfobuffer.portinfo.Port2_Type));
    ui->equit_type3->setCurrentIndex(decodePortTypeValue(dtuinfobuffer.portinfo.Port3_Type));
    ui->equit_type4->setCurrentIndex(decodePortTypeValue(dtuinfobuffer.portinfo.Port4_Type));
    ui->equit_type5->setCurrentIndex(decodePortTypeValue(dtuinfobuffer.portinfo.Port5_Type));
    ///<波特率
    fill_portbuand(dtuinfobuffer,1);
    fill_portbuand(dtuinfobuffer,2);
    fill_portbuand(dtuinfobuffer,3);
    fill_portbuand(dtuinfobuffer,4);
    fill_portbuand(dtuinfobuffer,5);
    ///<RF频点
    ui->RF1->setCurrentIndex(dtuinfobuffer.portinfo.Port1_RF);
    ui->RF2->setCurrentIndex(dtuinfobuffer.portinfo.Port2_RF);
    ui->RF3->setCurrentIndex(dtuinfobuffer.portinfo.Port3_RF);
    ui->RF4->setCurrentIndex(dtuinfobuffer.portinfo.Port4_RF);
    ui->RF5->setCurrentIndex(dtuinfobuffer.portinfo.Port5_RF);
    ///<保留项
    ui->keep1->setText(QString::number(dtuinfobuffer.portinfo.Port1_Reserve));
    ui->keep2->setText(QString::number(dtuinfobuffer.portinfo.Port2_Reserve));
    ui->keep3->setText(QString::number(dtuinfobuffer.portinfo.Port3_Reserve));
    ui->keep4->setText(QString::number(dtuinfobuffer.portinfo.Port4_Reserve));
    ui->keep5->setText(QString::number(dtuinfobuffer.portinfo.Port5_Reserve));
    m_RTU_ParameterSetting->recivemessage_lock.unlock();
}
