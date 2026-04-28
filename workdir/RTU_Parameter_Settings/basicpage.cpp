#include "basicpage.h"
#include "ui_BasicPage.h"
#include "bwprotocol.h"
#include "RTU_ParameterSetting.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <vector>

#define MAKEHWORD(a,b) ((uint16_t)(((uint16_t)(a) << 8) | ((uint16_t)(b))))

namespace {
QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:#2f78e8;color:#ffffff;border:none;border-radius:6px;padding:6px 14px;font-weight:600;min-height:32px;}"
        "QPushButton:hover{background:#2468cc;}"
        "QPushButton:pressed{background:#1d57aa;}");
}

QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:#ffffff;color:#2f78e8;border:1px solid #bfd4f6;border-radius:6px;padding:6px 14px;font-weight:600;min-height:32px;}"
        "QPushButton:hover{background:#edf4ff;}"
        "QPushButton:pressed{background:#dbeafe;}");
}

bool appendPackedDecimal(QByteArray &payload, uint8_t tag, uint8_t lenTag, const QString &text, int intDigits, int fracDigits)
{
    QString value = text.trimmed();
    if (value.isEmpty()) {
        return false;
    }

    const QRegularExpression pattern(
        fracDigits > 0
            ? QStringLiteral("^\\d{1,%1}(?:\\.\\d{1,%2})?$").arg(intDigits).arg(fracDigits)
            : QStringLiteral("^\\d{1,%1}$").arg(intDigits));
    if (!pattern.match(value).hasMatch()) {
        return false;
    }

    QStringList parts = value.split(QLatin1Char('.'));
    QString integerPart = parts.value(0);
    QString fractionPart = parts.size() > 1 ? parts.at(1) : QString();

    if (integerPart.size() > intDigits || fractionPart.size() > fracDigits) {
        return false;
    }

    integerPart = integerPart.rightJustified(intDigits, QLatin1Char('0'));
    fractionPart = fractionPart.leftJustified(fracDigits, QLatin1Char('0'));
    QString digits = integerPart + fractionPart;
    if (digits.size() % 2 != 0) {
        digits.prepend(QLatin1Char('0'));
    }

    QByteArray bytes = QByteArray::fromHex(digits.toLatin1());
    if (bytes.isEmpty()) {
        return false;
    }

    payload.append(static_cast<char>(tag));
    payload.append(static_cast<char>(lenTag));
    payload.append(bytes);
    return true;
}

void sendPayloadFrame(uint8_t afn, const QByteArray &payload)
{
    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),
                           afn,
                           STX,
                           static_cast<int16_t>(payload.size() + 8),
                           MTYPE_DOWN);
    pos += 22;
    memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte + pos, payload.constData(), payload.size());
    pos += payload.size();
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
}

QByteArray BasicPage::CJ_data;
QString BasicPage::str;
QByteArray BasicPage::TPzb;

BasicPage::BasicPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::BasicPage)
{
    ui->setupUi(this);
    ui->summaryCard->hide();
    setWindowTitle(QStringLiteral("基础参数"));
    ui->labelSectionBase->hide();
    ui->labelSectionTime->hide();

    const auto setLabelText = [this](const char *name, const QString &text) {
        if (QLabel *label = findChild<QLabel *>(QLatin1String(name))) {
            label->setText(text);
            label->setWordWrap(true);
        }
    };
    const auto setButtonText = [this](const char *name, const QString &text, const QString &style) {
        if (QPushButton *button = findChild<QPushButton *>(QLatin1String(name))) {
            button->setText(text);
            button->setStyleSheet(style);
        }
    };

    setLabelText("pageTitle", QStringLiteral("基础参数配置"));
    setLabelText("pageSubtitle", QStringLiteral("配置测站基础信息、工作模式和运行参数，并设置整点报送时段。"));
    setLabelText("labelDeviceId", QStringLiteral("设备编号："));
    setLabelText("labelModifyId", QStringLiteral("中心站地址："));
    setLabelText("labelWorkMode", QStringLiteral("工作方式："));
    setLabelText("labelPassword", QStringLiteral("通信密码："));
    setLabelText("labelReportInterval", QStringLiteral("加报间隔(分钟)："));
    setLabelText("labelDayStart", QStringLiteral("日起始时间(时)："));
    setLabelText("labelRainRange", QStringLiteral("雨量计量程(mm)："));
    setLabelText("labelStoreInterval", QStringLiteral("存储间隔(分钟)："));
    setLabelText("labelRainThreshold", QStringLiteral("雨量加报阈值(mm)："));
    setLabelText("labelWaterThreshold", QStringLiteral("水位加报阈值(m)："));
    setLabelText("labelHoursTitle", QStringLiteral("整点报送时段"));
    setLabelText("labelHoursHint", QStringLiteral("勾选需要参与整点报送的小时。"));
    setButtonText("huifu_Button", QStringLiteral("清空内容"), secondaryButtonStyle());
    setButtonText("read_Button", QStringLiteral("读取参数"), primaryButtonStyle());
    setButtonText("set_Button", QStringLiteral("设置参数"), primaryButtonStyle());
    setButtonText("selectAllHoursButton", QStringLiteral("全选"), secondaryButtonStyle());
    setButtonText("clearAllHoursButton", QStringLiteral("清空"), secondaryButtonStyle());
    setButtonText("every2HoursButton", QStringLiteral("每2小时"), secondaryButtonStyle());
    setButtonText("every3HoursButton", QStringLiteral("每3小时"), secondaryButtonStyle());

    ui->workModeCombo->addItems({
        QStringLiteral("自报确认模式"),
        QStringLiteral("自报模式"),
        QStringLiteral("查询应答模式"),
        QStringLiteral("兼容模式"),
        QStringLiteral("调试模式")
    });

    ui->workModeCombo->clear();
    ui->workModeCombo->addItems({
        QStringLiteral("自报确认模式"),
        QStringLiteral("自报模式"),
        QStringLiteral("查询应答模式"),
        QStringLiteral("兼容模式"),
        QStringLiteral("调试模式")
    });

    for (QLineEdit *edit : findChildren<QLineEdit *>()) {
        edit->setMinimumHeight(28);
    }
    for (QComboBox *combo : findChildren<QComboBox *>()) {
        combo->setMinimumHeight(28);
    }
    for (QPushButton *button : findChildren<QPushButton *>()) {
        button->setMinimumHeight(32);
    }
    for (QCheckBox *checkBox : hourCheckBoxes()) {
        checkBox->setStyleSheet(QStringLiteral("spacing:4px;padding:1px 2px;"));
    }

    clearFormValues();
}

BasicPage::~BasicPage()
{
    delete ui;
}

void BasicPage::clearFormValues()
{
    ui->deviceIdEdit->clear();
    ui->modifyIdEdit->clear();
    ui->workModeCombo->setCurrentIndex(-1);
    ui->tongxinmima->clear();
    ui->reportIntervalEdit->clear();
    ui->dayStartEdit->clear();
    ui->rainRangeEdit->clear();
    ui->storeIntervalEdit->clear();
    ui->rainThresholdEdit->clear();
    ui->waterThresholdEdit->clear();
    setHourSelection(0);
}

QList<QCheckBox *> BasicPage::hourCheckBoxes() const
{
    QList<QCheckBox *> list;
    for (int hour = 0; hour < 24; ++hour) {
        if (QCheckBox *checkBox = findChild<QCheckBox *>(QStringLiteral("time_%1").arg(hour))) {
            list.append(checkBox);
        }
    }
    return list;
}

void BasicPage::setHourSelection(int step, int startHour)
{
    const QList<QCheckBox *> checkBoxes = hourCheckBoxes();
    for (QCheckBox *checkBox : checkBoxes) {
        checkBox->setChecked(false);
    }

    if (step <= 0) {
        return;
    }

    for (int offset = 0; offset < 24; offset += step) {
        const int hour = (startHour + offset) % 24;
        if (QCheckBox *checkBox = findChild<QCheckBox *>(QStringLiteral("time_%1").arg(hour))) {
            checkBox->setChecked(true);
        }
    }
}

int BasicPage::selectedDayStartHour() const
{
    bool ok = false;
    const int value = ui->dayStartEdit->text().trimmed().toInt(&ok);
    if (ok && value >= 0 && value <= 23) {
        return value;
    }
    return 8;
}

void BasicPage::on_huifu_Button_clicked()
{
    clearFormValues();
}

void BasicPage::on_read_Button_clicked()
{
    m_RTU_ParameterSetting->baseReadRetriedAfterLearn = false;
    m_runtimeReadPending = true;
    requestBaseParamRead();
}

void BasicPage::requestBaseParamRead()
{
    uint32_t pos = 0;
    m_RTU_ParameterSetting->isbasedate = true;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage), AFN_READBASEPARAM, STX, 14, MTYPE_DOWN);
    pos += 22;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x03;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x10;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x0C;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x08;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x1E;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x1E;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    uint16_t crc_value = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte, pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos + 1] = (crc_value & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crc_value >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}

void BasicPage::requestRuntimeParamRead()
{
    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage), AFN_READRUNRPARAM, STX, 20, MTYPE_DOWN);
    pos += 22;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x21;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x08;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x22;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x08;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = static_cast<uint8_t>(0xA9);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x08;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x24;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x08;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x27;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x08;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x38;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = 0x12;
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

void BasicPage::setWorkMode(int workType)
{
    const int index = qBound(0, workType - 1, ui->workModeCombo->count() - 1);
    ui->workModeCombo->setCurrentIndex(index);
}

void BasicPage::checkBox_TPzb()
{
    TPzb.resize(24);
    for (int hour = 0; hour < 24; ++hour) {
        TPzb[hour] = 0;
        if (QCheckBox *checkBox = findChild<QCheckBox *>(QStringLiteral("time_%1").arg(hour))) {
            TPzb[hour] = checkBox->isChecked() ? 1 : 0;
        }
    }
}

void BasicPage::on_set_Button_clicked()
{
    checkBox_TPzb();

    const QString deviceIdText = ui->deviceIdEdit->text().trimmed().toUpper();
    const QString modifyIdText = ui->modifyIdEdit->text().trimmed();
    const QString passwordText = ui->tongxinmima->text().trimmed().toUpper();

    if (!QRegularExpression(QStringLiteral("^[0-9A-F]{10}$")).match(deviceIdText).hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("设备编号必须是10位十六进制，例如 0033336666"));
        return;
    }
    if (!QRegularExpression(QStringLiteral("^[0-9]{2}$")).match(modifyIdText).hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("中心站地址必须是2位数字，例如 10"));
        return;
    }
    if (!QRegularExpression(QStringLiteral("^[0-9A-F]{4}$")).match(passwordText).hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("通信密码必须是4位十六进制，例如 1234"));
        return;
    }
    if (ui->workModeCombo->currentIndex() < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择工作方式"));
        return;
    }
    if (ui->reportIntervalEdit->text().trimmed().isEmpty() ||
        ui->dayStartEdit->text().trimmed().isEmpty() ||
        ui->rainRangeEdit->text().trimmed().isEmpty() ||
        ui->storeIntervalEdit->text().trimmed().isEmpty() ||
        ui->rainThresholdEdit->text().trimmed().isEmpty() ||
        ui->waterThresholdEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("运行参数不能为空"));
        return;
    }

    QByteArray deviceIdBytes = QByteArray::fromHex(deviceIdText.toLatin1());
    if (deviceIdBytes.size() != 5) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("设备编号格式无效"));
        return;
    }

    bool ok = false;
    const int centerHigh = QString(modifyIdText.at(0)).toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("中心站地址格式无效"));
        return;
    }
    const int centerLow = QString(modifyIdText.at(1)).toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("中心站地址格式无效"));
        return;
    }
    const uint8_t centerAddr = static_cast<uint8_t>((centerHigh << 4) | centerLow);

    const uint16_t password = static_cast<uint16_t>(passwordText.toUShort(&ok, 16));
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("通信密码格式无效"));
        return;
    }

    QByteArray payload;
    payload.reserve(9 + TPzb.size());
    payload.append(static_cast<char>(0x01));
    payload.append(static_cast<char>(0x20));
    payload.append(static_cast<char>(centerAddr));
    payload.append(static_cast<char>(0x00));
    payload.append(static_cast<char>(0x00));
    payload.append(static_cast<char>(0x00));
    payload.append(static_cast<char>(0x02));
    payload.append(static_cast<char>(0x28));
    payload.append(deviceIdBytes);
    payload.append(static_cast<char>(0x0C));
    payload.append(static_cast<char>(0x08));
    payload.append(static_cast<char>(ui->workModeCombo->currentIndex() + 1));
    payload.append(static_cast<char>(0x03));
    payload.append(static_cast<char>(0x10));
    payload.append(static_cast<char>((password >> 8) & 0xff));
    payload.append(static_cast<char>(password & 0xff));
    payload.append(static_cast<char>(0x1E));

    QStringList enabledHours;
    for (int hour = 0; hour < 24; ++hour) {
        if (hour < TPzb.size() && TPzb.at(hour) != 0) {
            enabledHours.append(QString::number(hour));
            payload.append(static_cast<char>(hour));
        }
    }
    payload.insert(payload.size() - enabledHours.size(), static_cast<char>(enabledHours.size()));

    QByteArray runPayload;
    if (!appendPackedDecimal(runPayload, 0x21, 0x08, ui->reportIntervalEdit->text(), 2, 0)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("加报间隔格式无效，应为整数"));
        return;
    }
    if (!appendPackedDecimal(runPayload, 0x22, 0x08, ui->dayStartEdit->text(), 2, 0)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("日起始时间格式无效，应为整数"));
        return;
    }
    if (!appendPackedDecimal(runPayload, 0xA9, 0x08, ui->rainRangeEdit->text(), 2, 0)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("雨量计量程格式无效，应为整数"));
        return;
    }
    if (!appendPackedDecimal(runPayload, 0x24, 0x08, ui->storeIntervalEdit->text(), 2, 0)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("存储间隔格式无效，应为整数"));
        return;
    }
    if (!appendPackedDecimal(runPayload, 0x27, 0x08, ui->rainThresholdEdit->text(), 2, 0)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("雨量加报阈值格式无效，应为整数"));
        return;
    }
    if (!appendPackedDecimal(runPayload, 0x38, 0x12, ui->waterThresholdEdit->text(), 2, 2)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("水位加报阈值格式无效，应为最多两位整数加两位小数"));
        return;
    }

    sendPayloadFrame(AFN_BASEPARAM, payload);
    sendPayloadFrame(AFN_SETRUNPARAM, runPayload);

    qDebug().noquote()
        << "base params pending set:"
        << "deviceId=" << deviceIdText
        << "centerAddr=" << modifyIdText
        << "workMode=" << ui->workModeCombo->currentText()
        << "password=" << passwordText
        << "reportInterval=" << ui->reportIntervalEdit->text()
        << "dayStart=" << ui->dayStartEdit->text()
        << "rainRange=" << ui->rainRangeEdit->text()
        << "storeInterval=" << ui->storeIntervalEdit->text()
        << "rainThreshold=" << ui->rainThresholdEdit->text()
        << "waterThreshold=" << ui->waterThresholdEdit->text()
        << "hours=" << enabledHours.join(',');

    qDebug().noquote()
        << "run params pending set:"
        << "reportInterval=" << ui->reportIntervalEdit->text()
        << "dayStart=" << ui->dayStartEdit->text()
        << "rainRange=" << ui->rainRangeEdit->text()
        << "storeInterval=" << ui->storeIntervalEdit->text()
        << "rainThreshold=" << ui->rainThresholdEdit->text()
        << "waterThreshold=" << ui->waterThresholdEdit->text();
}

void BasicPage::on_selectAllHoursButton_clicked()
{
    for (QCheckBox *checkBox : hourCheckBoxes()) {
        checkBox->setChecked(true);
    }
}

void BasicPage::on_clearAllHoursButton_clicked()
{
    for (QCheckBox *checkBox : hourCheckBoxes()) {
        checkBox->setChecked(false);
    }
}

void BasicPage::on_every2HoursButton_clicked()
{
    setHourSelection(2, selectedDayStartHour());
}

void BasicPage::on_every3HoursButton_clicked()
{
    setHourSelection(3, 8);
}

void BasicPage::handle_baseConfigParam()
{
    for (QCheckBox *checkBox : hourCheckBoxes()) {
        checkBox->setCheckState(Qt::Unchecked);
    }

    m_RTU_ParameterSetting->recivemessage_lock.lock();
    uint16_t datelen = MAKEHWORD(*(m_RTU_ParameterSetting->m_recivemessage.messageByte + 11),
                                 *(m_RTU_ParameterSetting->m_recivemessage.messageByte + 12)) + 15;
    std::vector<uint8_t> handledate(m_RTU_ParameterSetting->m_recivemessage.messageByte + 22,
                                    m_RTU_ParameterSetting->m_recivemessage.messageByte + datelen - 1);
    uint index = 0;
    while (index < handledate.size()) {
        if (handledate.at(index) == 0xf1 || handledate.at(index) == 0xf0) {
            index++;
            if (index < handledate.size() && handledate.at(index) == handledate.at(index - 1)) {
                index++;
                if (index + 5 <= handledate.size()) {
                    index += 5;
                } else {
                    break;
                }
            } else {
                break;
            }
        } else if (handledate.at(index) == 0x1e) {
            index++;
            int len = handledate.at(index);
            if (index + len <= handledate.size()) {
                index++;
                for (int i = 0; i < len; i++) {
                    if (QCheckBox *chbox = findChild<QCheckBox *>(QStringLiteral("time_%1").arg(handledate[index + i]))) {
                        chbox->setCheckState(Qt::Checked);
                    }
                }
                index += len;
            } else {
                break;
            }
        } else if (handledate.at(index) == 0x01) {
            index++;
            int len = handledate.at(index) >> 3;
            if (index + len < handledate.size()) {
                index++;
                if (len == 4) {
                    m_RTU_ParameterSetting->FrameBaseInfo.centeraddr = handledate.at(index);
                    setBaseFrameInfo(m_RTU_ParameterSetting->FrameBaseInfo);
                }
                index += len;
            } else {
                break;
            }
        } else if (handledate.at(index) == 0x02) {
            index++;
            int len = handledate.at(index) >> 3;
            if (index + len < handledate.size()) {
                index++;
                if (len == 5) {
                    memcpy(m_RTU_ParameterSetting->FrameBaseInfo.testaddr, handledate.data() + index, 5);
                    setBaseFrameInfo(m_RTU_ParameterSetting->FrameBaseInfo);
                }
                index += len;
            } else {
                break;
            }
        } else if (handledate.at(index) == 0x03) {
            index++;
            int len = handledate.at(index) >> 3;
            if (index + len < handledate.size()) {
                index++;
                if (len == 2) {
                    m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword =
                        (static_cast<uint16_t>(handledate.at(index)) << 8) |
                        static_cast<uint16_t>(handledate.at(index + 1));
                    setBaseFrameInfo(m_RTU_ParameterSetting->FrameBaseInfo);
                }
                index += len;
            } else {
                break;
            }
        } else if (handledate.at(index) == 0x0C) {
            index++;
            int len = handledate.at(index) >> 3;
            if (index + len < handledate.size()) {
                index++;
                if (len == 1) {
                    m_RTU_ParameterSetting->FrameBaseInfo.worktype = handledate.at(index);
                    setWorkMode(handledate.at(index));
                }
                index += len;
            } else {
                break;
            }
        } else {
            index++;
            if (index >= handledate.size()) {
                break;
            }
            int len = handledate.at(index) >> 3;
            if (index + len <= handledate.size()) {
                index++;
                index += len;
            } else {
                break;
            }
        }
    }
    m_RTU_ParameterSetting->recivemessage_lock.unlock();

    if (m_runtimeReadPending) {
        m_runtimeReadPending = false;
        requestRuntimeParamRead();
    }
}

void BasicPage::handle_runtimeConfigParam()
{
    m_RTU_ParameterSetting->recivemessage_lock.lock();
    const uint16_t datelen = MAKEHWORD(*(m_RTU_ParameterSetting->m_recivemessage.messageByte + 11),
                                       *(m_RTU_ParameterSetting->m_recivemessage.messageByte + 12)) + 15;
    std::vector<uint8_t> handledate(m_RTU_ParameterSetting->m_recivemessage.messageByte + 22,
                                    m_RTU_ParameterSetting->m_recivemessage.messageByte + datelen - 1);

    uint index = 0;
    while (index < handledate.size()) {
        if (handledate.at(index) == 0xf1 || handledate.at(index) == 0xf0) {
            index++;
            if (index < handledate.size() && handledate.at(index) == handledate.at(index - 1)) {
                index++;
                if (index + 5 <= handledate.size()) {
                    index += 5;
                } else {
                    break;
                }
            } else {
                break;
            }
        } else {
            if (index + 1 >= handledate.size()) {
                break;
            }
            const uint8_t tag = handledate.at(index);
            const int byteLen = handledate.at(index + 1) >> 3;
            index += 2;
            if (index + byteLen > handledate.size()) {
                break;
            }

            switch (tag) {
            case 0x21:
                ui->reportIntervalEdit->setText(decodePackedDecimal(handledate, index, byteLen, 0));
                break;
            case 0x22:
                ui->dayStartEdit->setText(decodePackedDecimal(handledate, index, byteLen, 0));
                break;
            case 0xA9:
                ui->rainRangeEdit->setText(decodePackedDecimal(handledate, index, byteLen, 0));
                break;
            case 0x24:
                ui->storeIntervalEdit->setText(decodePackedDecimal(handledate, index, byteLen, 0));
                break;
            case 0x27:
                ui->rainThresholdEdit->setText(decodePackedDecimal(handledate, index, byteLen, 0));
                break;
            case 0x38:
                ui->waterThresholdEdit->setText(decodePackedDecimal(handledate, index, byteLen, 2));
                break;
            default:
                break;
            }
            index += byteLen;
        }
    }
    m_RTU_ParameterSetting->recivemessage_lock.unlock();
}

QString BasicPage::testAddrToDeviceId(const uint8_t testaddr[5]) const
{
    return QByteArray(reinterpret_cast<const char *>(testaddr), 5).toHex().toUpper();
}

QString BasicPage::decodePackedDecimal(const std::vector<uint8_t> &data, uint index, int byteLen, int fracDigits) const
{
    QString digits;
    digits.reserve(byteLen * 2);
    for (int offset = 0; offset < byteLen && index + offset < data.size(); ++offset) {
        const uint8_t value = data.at(index + offset);
        digits.append(QChar('0' + ((value >> 4) & 0x0f)));
        digits.append(QChar('0' + (value & 0x0f)));
    }

    if (fracDigits > 0) {
        while (digits.size() <= fracDigits) {
            digits.prepend(QLatin1Char('0'));
        }
        digits.insert(digits.size() - fracDigits, QLatin1Char('.'));
        return QString::number(digits.toDouble(), 'f', fracDigits);
    }

    bool ok = false;
    return QString::number(digits.toInt(&ok, 10));
}

void BasicPage::setBaseFrameInfo(const s_FrameBaseInfo &date)
{
    ui->modifyIdEdit->setText(QString::number((date.centeraddr >> 4) & 0x0f) + QString::number(date.centeraddr & 0x0f));
    ui->tongxinmima->setText(QStringLiteral("%1").arg(date.cmcpassword, 4, 16, QLatin1Char('0')).toUpper());
    ui->deviceIdEdit->setText(testAddrToDeviceId(date.testaddr));

    m_RTU_ParameterSetting->displayInfo.centeraddr = date.centeraddr;
    m_RTU_ParameterSetting->displayInfo.cmcpassword = date.cmcpassword;
    memcpy(m_RTU_ParameterSetting->displayInfo.testaddr, date.testaddr, 5);
}
