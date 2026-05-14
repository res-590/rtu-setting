#include "dtuset.h"
#include "RTU_ParameterSetting.h"
#include "ui_dtuset.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QDebug>
#include <QFrame>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStringList>
#include <QVBoxLayout>

namespace {
QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #3f7fe8,stop:1 #2f6fdc);}"
        "QPushButton{color:#ffffff;border:1px solid #2a63c4;border-radius:10px;padding:10px 18px;font-weight:700;min-height:44px;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #4d8bf0,stop:1 #3678e4);}"
        "QPushButton:pressed{background:#295fc0;}");
}

QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:#f7faff;color:#2f6fdc;border:1px solid #bdd1f4;border-radius:10px;padding:10px 14px;font-weight:700;min-height:40px;}"
        "QPushButton:hover{background:#eef4ff;border:1px solid #9ebef0;}"
        "QPushButton:pressed{background:#dde9ff;}");
}

QString cardStyle()
{
    return QStringLiteral("background:#ffffff;border:1px solid #dbe6f7;border-radius:18px;");
}

QString panelStyle()
{
    return QStringLiteral(
        "QWidget#dtuset{background:#f4f7fb;}"
        "QScrollArea{background:transparent;border:none;}"
        "QLabel{color:#334155;font-size:13px;}"
        "QLabel#pageTitle{font-size:28px;font-weight:700;color:#0f172a;}"
        "QLabel#labelBaseTitle,QLabel#channelSectionTitle{font-size:18px;font-weight:700;color:#0f172a;}"
        "QLabel#channel1Title,QLabel#channel2Title,QLabel#channel3Title,QLabel#channel4Title{font-size:17px;font-weight:700;color:#0f172a;}"
        "QLabel#channel1Hint,QLabel#channel2Hint,QLabel#channel3Hint,QLabel#channel4Hint{color:#64748b;font-size:12px;line-height:1.6;}"
        "QLineEdit,QComboBox{background:#f8fafc;border:1px solid #cbd5e1;border-radius:10px;padding:8px 12px;color:#0f172a;selection-background-color:#bfdbfe;}"
        "QLineEdit:focus,QComboBox:focus{border:1px solid #3b82f6;background:#ffffff;}"
        "QLineEdit::placeholder{color:#94a3b8;}"
        "QComboBox::drop-down{border:none;width:30px;}"
        "QComboBox::down-arrow{image:url(:/branding/combo_arrow_down.svg);width:12px;height:8px;margin-right:10px;}"
        "QFrame#channel1Card,QFrame#channel2Card,QFrame#channel3Card,QFrame#channel4Card{background:#f8fbff;border:1px solid #d9e7fb;border-radius:16px;}");
}
}

dtuset::dtuset(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::dtuset)
{
    ui->setupUi(this);
    setStyleSheet(panelStyle());
    setWindowTitle(QStringLiteral("DTU 参数"));

    const auto setLabelText = [this](const char *name, const QString &text) {
        if (QLabel *label = findChild<QLabel *>(QLatin1String(name))) {
            label->setText(text);
            label->setWordWrap(true);
        }
    };

    setLabelText("pageTitle", QStringLiteral("DTU 参数配置"));

    if (QPushButton *button = findChild<QPushButton *>(QStringLiteral("read_Button"))) {
        button->setText(QStringLiteral("读取参数"));
        button->setStyleSheet(primaryButtonStyle());
        button->setMinimumHeight(44);
    }
    if (QPushButton *button = findChild<QPushButton *>(QStringLiteral("set_Button"))) {
        button->setText(QStringLiteral("写入参数"));
        button->setStyleSheet(primaryButtonStyle());
        button->setMinimumHeight(44);
    }
    if (QPushButton *setButton = findChild<QPushButton *>(QStringLiteral("set_Button"))) {
        QPushButton *clearButton = new QPushButton(QStringLiteral("清空内容"), setButton->parentWidget());
        clearButton->setObjectName(QStringLiteral("clearButton"));
        clearButton->setStyleSheet(secondaryButtonStyle());
        clearButton->setMinimumHeight(40);
        if (QBoxLayout *layout = qobject_cast<QBoxLayout *>(setButton->parentWidget()->layout())) {
            if (QPushButton *readButton = findChild<QPushButton *>(QStringLiteral("read_Button"))) {
                layout->insertWidget(layout->indexOf(readButton), clearButton);
            } else {
                layout->insertWidget(layout->indexOf(setButton), clearButton);
            }
        }
        connect(clearButton, &QPushButton::clicked, this, &dtuset::handleClearButtonClicked);
    }

    ui->summaryCard->hide();
    if (ui->baseConfigCard != nullptr) {
        ui->baseConfigCard->setStyleSheet(cardStyle());
    }
    if (ui->actionCard != nullptr) {
        ui->actionCard->setStyleSheet(QStringLiteral("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f8fbff,stop:1 #eef5ff);border:1px solid #cfe0fb;border-radius:18px;"));
    }
    if (QFrame *channelSectionCard = findChild<QFrame *>(QStringLiteral("channelSectionCard"))) {
        channelSectionCard->setStyleSheet(cardStyle());
    }
    if (QFrame *actionCard = findChild<QFrame *>(QStringLiteral("actionCard"))) {
        actionCard->setMinimumWidth(250);
        actionCard->setMaximumWidth(300);
    }
    if (ui->actionCard != nullptr) {
        if (QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(ui->actionCard->layout())) {
            layout->addStretch(1);
        }
    }

    for (QLineEdit *edit : findChildren<QLineEdit *>()) {
        edit->setMinimumHeight(42);
    }
    for (QComboBox *combo : findChildren<QComboBox *>()) {
        combo->setMinimumHeight(42);
    }
    for (QPushButton *button : findChildren<QPushButton *>()) {
        button->setMinimumHeight(qMax(button->minimumHeight(), 40));
    }

    const auto applyIntValidator = [this](const char *name, int min, int max) {
        if (QLineEdit *edit = findChild<QLineEdit *>(QLatin1String(name))) {
            edit->setValidator(new QIntValidator(min, max, edit));
        }
    };
    const QRegularExpression ipPattern(QStringLiteral(R"(^(25[0-5]|2[0-4]\d|1?\d?\d)(\.(25[0-5]|2[0-4]\d|1?\d?\d)){0,3}$)"));
    for (const QString &name : {QStringLiteral("IP1"), QStringLiteral("IP2"), QStringLiteral("IP3"), QStringLiteral("IP4")}) {
        if (QLineEdit *edit = findChild<QLineEdit *>(name)) {
            edit->setValidator(new QRegularExpressionValidator(ipPattern, edit));
        }
    }
    applyIntValidator("Max_data", 0, 65535);
    applyIntValidator("port1", 0, 65535);
    applyIntValidator("port2", 0, 65535);
    applyIntValidator("port3", 0, 65535);
    applyIntValidator("port4", 0, 65535);

    memset(&dtuinfo.DTU_info, 0, sizeof(dtuinfo.DTU_info));
    hide();
}
dtuset::~dtuset()
{
    delete ui;
}

void dtuset::syncUiToProtocol()
{
    on_run_model_currentIndexChanged(ui->run_model->currentIndex());
    on_baud_rate_currentIndexChanged(ui->baud_rate->currentIndex());
    on_Max_data_editingFinished();
    on_SMS_editingFinished();
    on_sms_code_currentIndexChanged(ui->sms_code->currentIndex());

    on_channel1_currentIndexChanged(ui->channel1->currentIndex());
    on_channel2_currentIndexChanged(ui->channel2->currentIndex());
    on_channel3_currentIndexChanged(ui->channel3->currentIndex());
    on_channel4_currentIndexChanged(ui->channel4->currentIndex());

    on_IP1_editingFinished();
    on_IP2_editingFinished();
    on_IP3_editingFinished();
    on_IP4_editingFinished();

    on_domain1_editingFinished();
    on_domain2_editingFinished();
    on_domain3_editingFinished();
    on_domain4_editingFinished();

    on_port1_editingFinished();
    on_port2_editingFinished();
    on_port3_editingFinished();
    on_port4_editingFinished();

    on_sms_num1_editingFinished();
    on_sms_num2_editingFinished();
    on_sms_num3_editingFinished();
    on_sms_num4_editingFinished();
}

void dtuset::clearDisplayedParameters()
{
    memset(&dtuinfo.DTU_info, 0, sizeof(dtuinfo.DTU_info));

    ui->run_model->setCurrentIndex(0);
    ui->baud_rate->setCurrentIndex(0);
    ui->sms_code->setCurrentIndex(0);

    ui->Max_data->clear();
    ui->SMS->clear();

    ui->channel1->setCurrentIndex(0);
    ui->channel2->setCurrentIndex(0);
    ui->channel3->setCurrentIndex(0);
    ui->channel4->setCurrentIndex(0);

    ui->IP1->clear();
    ui->IP2->clear();
    ui->IP3->clear();
    ui->IP4->clear();

    ui->domain1->clear();
    ui->domain2->clear();
    ui->domain3->clear();
    ui->domain4->clear();

    ui->port1->clear();
    ui->port2->clear();
    ui->port3->clear();
    ui->port4->clear();

    ui->sms_num1->clear();
    ui->sms_num2->clear();
    ui->sms_num3->clear();
    ui->sms_num4->clear();
}
//void dtuset::SwapDateByte(uint8_t *date,uint8_t n)
//{
//    uint8_t temp;
//    uint8_t head = 0,tail = n-1;
//    while(head < tail){
//        temp = *(date + head);
//        *(date + head) = *(date + tail);
//        *(date + tail) = temp;
//        head++;
//        tail--;
//    }
//}
void dtuset::on_set_Button_clicked()
{
    syncUiToProtocol();
    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte,0,sizeof (m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),AFN_SETRTUPARAM,STX,sizeof(this->dtuinfo.DTU_info) + 8,MTYPE_DOWN);
    pos += 22;
    memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte+pos,this->dtuinfo.date,sizeof(this->dtuinfo.DTU_info));
    pos += sizeof(this->dtuinfo.DTU_info);
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

void dtuset::handleClearButtonClicked()
{
    clearDisplayedParameters();
}

void dtuset::on_channel1_currentIndexChanged(int index)
{
    this->dtuinfo.DTU_info.ch1_in_type = index - 1;
}

void dtuset::on_channel2_currentIndexChanged(int index)
{
    this->dtuinfo.DTU_info.ch2_in_type = index - 1;
}

void dtuset::on_channel3_currentIndexChanged(int index)
{
    this->dtuinfo.DTU_info.ch3_in_type = index - 1;
}

void dtuset::on_channel4_currentIndexChanged(int index)
{
    this->dtuinfo.DTU_info.ch4_in_type = index - 1;
}

void dtuset::on_IP1_editingFinished()
{
    ///<ip字符串转为16进制
    QStringList strlist = ui->IP1->text().split('.');
    if(strlist.count() < 4) return;
    this->dtuinfo.DTU_info.ch1_in_ipaddr = strlist[0].toUInt();
    this->dtuinfo.DTU_info.ch1_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch1_in_ipaddr += strlist[1].toUInt();
    this->dtuinfo.DTU_info.ch1_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch1_in_ipaddr += strlist[2].toUInt();
    this->dtuinfo.DTU_info.ch1_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch1_in_ipaddr += strlist[3].toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch1_in_ipaddr),sizeof(this->dtuinfo.DTU_info.ch1_in_ipaddr));
}

void dtuset::on_IP2_editingFinished()
{
    QStringList strlist = ui->IP2->text().split('.');
    if(strlist.count() < 4) return;
    this->dtuinfo.DTU_info.ch2_in_ipaddr = strlist[0].toUInt();
    this->dtuinfo.DTU_info.ch2_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch2_in_ipaddr += strlist[1].toUInt();
    this->dtuinfo.DTU_info.ch2_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch2_in_ipaddr += strlist[2].toUInt();
    this->dtuinfo.DTU_info.ch2_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch2_in_ipaddr += strlist[3].toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch2_in_ipaddr),sizeof(this->dtuinfo.DTU_info.ch2_in_ipaddr));
}

void dtuset::on_IP3_editingFinished()
{
    QStringList strlist = ui->IP3->text().split('.');
    if(strlist.count() < 4) return;
    this->dtuinfo.DTU_info.ch3_in_ipaddr = strlist[0].toUInt();
    this->dtuinfo.DTU_info.ch3_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch3_in_ipaddr += strlist[1].toUInt();
    this->dtuinfo.DTU_info.ch3_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch3_in_ipaddr += strlist[2].toUInt();
    this->dtuinfo.DTU_info.ch3_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch3_in_ipaddr += strlist[3].toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch3_in_ipaddr),sizeof(this->dtuinfo.DTU_info.ch3_in_ipaddr));
}

void dtuset::on_IP4_editingFinished()
{
    QStringList strlist = ui->IP4->text().split('.');
    if(strlist.count() < 4) return;
    this->dtuinfo.DTU_info.ch4_in_ipaddr = strlist[0].toUInt();
    this->dtuinfo.DTU_info.ch4_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch4_in_ipaddr += strlist[1].toUInt();
    this->dtuinfo.DTU_info.ch4_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch4_in_ipaddr += strlist[2].toUInt();
    this->dtuinfo.DTU_info.ch4_in_ipaddr <<= 8;
    this->dtuinfo.DTU_info.ch4_in_ipaddr += strlist[3].toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch4_in_ipaddr),sizeof(this->dtuinfo.DTU_info.ch4_in_ipaddr));
}

void dtuset::on_domain1_editingFinished()
{
    QByteArray Block = ui->domain1->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch1_in_dname,0,sizeof(this->dtuinfo.DTU_info.ch1_in_dname));
    memcpy(this->dtuinfo.DTU_info.ch1_in_dname,Block.data(),Block.size()+1);
}

void dtuset::on_domain2_editingFinished()
{
    QByteArray Block = ui->domain2->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch2_in_dname,0,sizeof(this->dtuinfo.DTU_info.ch2_in_dname));
    memcpy(this->dtuinfo.DTU_info.ch2_in_dname,Block.data(),Block.size()+1);
}

void dtuset::on_domain3_editingFinished()
{
    QByteArray Block = ui->domain3->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch3_in_dname,0,sizeof(this->dtuinfo.DTU_info.ch3_in_dname));
    memcpy(this->dtuinfo.DTU_info.ch3_in_dname,Block.data(),Block.size()+1);
}

void dtuset::on_domain4_editingFinished()
{
    QByteArray Block = ui->domain4->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch4_in_dname,0,sizeof(this->dtuinfo.DTU_info.ch4_in_dname));
    memcpy(this->dtuinfo.DTU_info.ch4_in_dname,Block.data(),Block.size()+1);
}

void dtuset::on_port1_editingFinished()
{
    this->dtuinfo.DTU_info.ch1_in_port = ui->port1->text().toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch1_in_port),sizeof(this->dtuinfo.DTU_info.ch1_in_port));
}


void dtuset::on_port2_editingFinished()
{
    this->dtuinfo.DTU_info.ch2_in_port = ui->port2->text().toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch2_in_port),sizeof(this->dtuinfo.DTU_info.ch2_in_port));
}

void dtuset::on_port3_editingFinished()
{
    this->dtuinfo.DTU_info.ch3_in_port = ui->port3->text().toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch3_in_port),sizeof(this->dtuinfo.DTU_info.ch3_in_port));
}

void dtuset::on_port4_editingFinished()
{
    this->dtuinfo.DTU_info.ch4_in_port = ui->port4->text().toUInt();
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.ch4_in_port),sizeof(this->dtuinfo.DTU_info.ch4_in_port));
}

void dtuset::on_sms_num1_editingFinished()
{
    QByteArray Block = ui->sms_num1->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch1_number,0,sizeof(this->dtuinfo.DTU_info.ch1_number));
    memcpy(this->dtuinfo.DTU_info.ch1_number,Block.data(),Block.size()+1);
}

void dtuset::on_sms_num2_editingFinished()
{
    QByteArray Block = ui->sms_num2->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch2_number,0,sizeof(this->dtuinfo.DTU_info.ch2_number));
    memcpy(this->dtuinfo.DTU_info.ch2_number,Block.data(),Block.size()+1);
}

void dtuset::on_sms_num3_editingFinished()
{
    QByteArray Block = ui->sms_num3->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch3_number,0,sizeof(this->dtuinfo.DTU_info.ch3_number));
    memcpy(this->dtuinfo.DTU_info.ch3_number,Block.data(),Block.size()+1);
}

void dtuset::on_sms_num4_editingFinished()
{
    QByteArray Block = ui->sms_num4->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.ch4_number,0,sizeof(this->dtuinfo.DTU_info.ch4_number));
    memcpy(this->dtuinfo.DTU_info.ch4_number,Block.data(),Block.size()+1);
}

void dtuset::on_run_model_currentIndexChanged(int index)
{
    this->dtuinfo.DTU_info.run_model = index - 1;
}

void dtuset::on_baud_rate_currentIndexChanged(int index)
{
    this->dtuinfo.DTU_info.baundrate = 0;
    switch (index) {
        case 0:{
        }break;
        case 1:{        ///<300
            uint32_t buad = 300;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 2:{        ///<600
            uint32_t buad = 600;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 3:{        ///<1200
            uint32_t buad = 1200;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 4:{        ///<2400
            uint32_t buad = 2400;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 5:{        ///<4800
            uint32_t buad = 4800;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 6:{        ///<9600
            uint32_t buad = 9600;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 7:{        ///<19200
            uint32_t buad = 19200;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 8:{        ///<38400
            uint32_t buad = 38400;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 9:{        ///<57600
            uint32_t buad = 57600;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        case 10:{       ///<115200
            uint32_t buad = 115200;
            this->dtuinfo.DTU_info.baundrate = buad;
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
    SwapDateByte((uint8_t*) &(this->dtuinfo.DTU_info.baundrate),sizeof(this->dtuinfo.DTU_info.baundrate));
}

void dtuset::on_Max_data_editingFinished()
{
    this->dtuinfo.DTU_info.Max_Datepacket = ui->Max_data->text().toUInt();
//    this->dtuinfo.DTU_info = ui->port4->text().toUInt();
    SwapDateByte((uint8_t *)&(this->dtuinfo.DTU_info.Max_Datepacket),sizeof(this->dtuinfo.DTU_info.Max_Datepacket));
}

void dtuset::on_SMS_editingFinished()
{
    QByteArray Block = ui->SMS->text().toLatin1();
    ///<清空域名，未使用字段填充为0
    memset(this->dtuinfo.DTU_info.server_number,0,sizeof(this->dtuinfo.DTU_info.server_number));
    memcpy(this->dtuinfo.DTU_info.server_number,Block.data(),Block.size()+1);
}

void dtuset::on_sms_code_currentIndexChanged(int index)
{
    this->dtuinfo.DTU_info.message_CompileType = index - 1;
}

void dtuset::on_read_Button_clicked()
{
    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte,0,sizeof (m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),AFN_READRTUPARAM,STX,8,MTYPE_DOWN);
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
void dtuset::handle_DTUINFO(void)
{
    RTU_Protocol dtuinfobuffer;
    m_RTU_ParameterSetting->recivemessage_lock.lock();
    QString buffer;
    memcpy(dtuinfobuffer.date,m_RTU_ParameterSetting->m_recivemessage.messageByte + 22,sizeof (dtuinfobuffer.date));
    m_RTU_ParameterSetting->recivemessage_lock.unlock();
    ///<运行模式
    ui->run_model->setCurrentIndex(dtuinfobuffer.DTU_info.run_model + 1);
    ///<最大包数
    SwapDateByte((uint8_t *)&(dtuinfobuffer.DTU_info.Max_Datepacket),sizeof(dtuinfobuffer.DTU_info.Max_Datepacket));
    buffer = QString::number(dtuinfobuffer.DTU_info.Max_Datepacket);
    ui->Max_data->setText(buffer);
    ///<中心服务号
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.server_number);
    ui->SMS->setText(buffer);
    ///<波特率
    SwapDateByte((uint8_t*) &(dtuinfobuffer.DTU_info.baundrate),sizeof(dtuinfobuffer.DTU_info.baundrate));
    switch (dtuinfobuffer.DTU_info.baundrate) {
        case 0:{
            ui->baud_rate->setCurrentIndex(0);
        }break;
        case 300:{
            ui->baud_rate->setCurrentIndex(1);
        }break;
        case 600:{
            ui->baud_rate->setCurrentIndex(2);
        }break;
        case 1200:{
            ui->baud_rate->setCurrentIndex(3);
        }break;
        case 2400:{
            ui->baud_rate->setCurrentIndex(4);
        }break;
        case 4800:{
            ui->baud_rate->setCurrentIndex(5);
        }break;
        case 9600:{
            ui->baud_rate->setCurrentIndex(6);
        }break;
        case 19200:{
            ui->baud_rate->setCurrentIndex(7);
        }break;
        case 38400:{
            ui->baud_rate->setCurrentIndex(8);
        }break;
        case 57600:{
            ui->baud_rate->setCurrentIndex(9);
        }break;
        case 115200:{
            ui->baud_rate->setCurrentIndex(10);
        }break;
        default:
            qDebug()<<"baudrate set error\r\n";
            break;
    }
    ///<传输协议
    ui->channel1->setCurrentIndex(dtuinfobuffer.DTU_info.ch1_in_type + 1);
    ui->channel2->setCurrentIndex(dtuinfobuffer.DTU_info.ch2_in_type + 1);
    ui->channel3->setCurrentIndex(dtuinfobuffer.DTU_info.ch3_in_type + 1);
    ui->channel4->setCurrentIndex(dtuinfobuffer.DTU_info.ch4_in_type + 1);
    ///<IP地址
    buffer = QString::number((uint8_t)(dtuinfobuffer.DTU_info.ch1_in_ipaddr&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch1_in_ipaddr >> 8)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch1_in_ipaddr >> 16)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch1_in_ipaddr >> 24)&0xff));
    ui->IP1->setText(buffer);
    buffer = QString::number((uint8_t)(dtuinfobuffer.DTU_info.ch2_in_ipaddr&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch2_in_ipaddr >> 8)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch2_in_ipaddr >> 16)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch2_in_ipaddr >> 24)&0xff));
    ui->IP2->setText(buffer);
    buffer = QString::number((uint8_t)(dtuinfobuffer.DTU_info.ch3_in_ipaddr&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch3_in_ipaddr >> 8)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch3_in_ipaddr >> 16)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch3_in_ipaddr >> 24)&0xff));
    ui->IP3->setText(buffer);
    buffer = QString::number((uint8_t)(dtuinfobuffer.DTU_info.ch4_in_ipaddr&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch4_in_ipaddr >> 8)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch4_in_ipaddr >> 16)&0xff));
    buffer += '.';
    buffer += QString::number((uint8_t)((dtuinfobuffer.DTU_info.ch4_in_ipaddr >> 24)&0xff));
    ui->IP4->setText(buffer);
    ///<域名
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch1_in_dname);
    ui->domain1->setText(buffer);
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch2_in_dname);
    ui->domain2->setText(buffer);
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch3_in_dname);
    ui->domain3->setText(buffer);
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch4_in_dname);
    ui->domain4->setText(buffer);
    ///端口
    SwapDateByte((uint8_t*) &(dtuinfobuffer.DTU_info.ch1_in_port),sizeof(dtuinfobuffer.DTU_info.ch1_in_port));
    buffer = QString::number(dtuinfobuffer.DTU_info.ch1_in_port);
    ui->port1->setText(buffer);
    SwapDateByte((uint8_t*) &(dtuinfobuffer.DTU_info.ch2_in_port),sizeof(dtuinfobuffer.DTU_info.ch2_in_port));
    buffer = QString::number(dtuinfobuffer.DTU_info.ch2_in_port);
    ui->port2->setText(buffer);
    SwapDateByte((uint8_t*) &(dtuinfobuffer.DTU_info.ch3_in_port),sizeof(dtuinfobuffer.DTU_info.ch3_in_port));
    buffer = QString::number(dtuinfobuffer.DTU_info.ch3_in_port);
    ui->port3->setText(buffer);
    SwapDateByte((uint8_t*) &(dtuinfobuffer.DTU_info.ch4_in_port),sizeof(dtuinfobuffer.DTU_info.ch4_in_port));
    buffer = QString::number(dtuinfobuffer.DTU_info.ch4_in_port);
    ui->port4->setText(buffer);
    ///<服务号码
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch1_number);
    ui->sms_num1->setText(buffer);
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch2_number);
    ui->sms_num2->setText(buffer);
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch3_number);
    ui->sms_num3->setText(buffer);
    buffer = QString::fromLocal8Bit((const char *)dtuinfobuffer.DTU_info.ch4_number);
    ui->sms_num4->setText(buffer);
    ///<短信编码
    ui->sms_code->setCurrentIndex(dtuinfobuffer.DTU_info.message_CompileType+1);
    this->dtuinfo = dtuinfobuffer;
//this->dtuinfo.DTU_info.message_CompileType = index - 1;
}

