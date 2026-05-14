#include "dataquerypage.h"

#include "RTU_ParameterSetting.h"
#include "yunxingcanshu.h"

#include <QAbstractItemView>
#include <QBuffer>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

#include <cstring>

namespace {
struct DecodedRealtimeData
{
    bool ok = false;
    bool isImage = false;
    QString valueText;
    QString unitText;
    QString statusText;
    QByteArray imagePayload;
    QString sampleTimeText;
};

QString zh(const char *text)
{
    return QString::fromUtf8(text);
}

SensorRecord makeBatteryVoltageRecord()
{
    SensorRecord record;
    record.id = QStringLiteral("builtin_battery_voltage");
    record.name = zh(u8"蓄电池电压");
    record.element = zh(u8"蓄电池电压");
    record.model = zh(u8"设备内置");
    record.port = zh(u8"内置采集");
    record.elementCode = 0x38;
    return record;
}

bool isVirtualBatteryRecord(const SensorRecord &record)
{
    return record.id == QStringLiteral("builtin_battery_voltage");
}

quint8 hiOctet(quint8 value)
{
    return static_cast<quint8>((value >> 4) & 0x0f);
}

quint8 loOctet(quint8 value)
{
    return static_cast<quint8>(value & 0x0f);
}

int decodePackedPacketTotal(quint8 b14, quint8 b15)
{
    const int lowByte = (static_cast<int>(loOctet(b14)) << 4) | static_cast<int>(hiOctet(b15));
    const int highByte = static_cast<int>(hiOctet(b14));
    return (highByte << 8) | lowByte;
}

int decodePackedPacketPos(quint8 b15, quint8 b16)
{
    const int lowByte = (static_cast<int>(hiOctet(b16)) << 4) | static_cast<int>(loOctet(b16));
    const int highByte = static_cast<int>(loOctet(b15));
    return (highByte << 8) | lowByte;
}

bool hasJpegStartMarker(const QByteArray &payload)
{
    return payload.size() >= 2 &&
           static_cast<quint8>(payload.at(0)) == 0xFF &&
           static_cast<quint8>(payload.at(1)) == 0xD8;
}

bool containsJpegEndMarker(const QByteArray &payload)
{
    return payload.indexOf(QByteArray::fromHex("FFD9")) >= 0;
}

QByteArray normalizeJpegPayload(const QByteArray &payload)
{
    if (payload.isEmpty()) {
        return {};
    }

    const int startPos = payload.indexOf(QByteArray::fromHex("FFD8"));
    if (startPos < 0) {
        return payload;
    }

    const int endPos = payload.indexOf(QByteArray::fromHex("FFD9"), startPos + 2);
    if (endPos < 0) {
        return payload.mid(startPos);
    }

    return payload.mid(startPos, endPos - startPos + 2);
}

bool isAllFF(const QByteArray &payload)
{
    for (char byte : payload) {
        if (static_cast<quint8>(byte) != 0xff) {
            return false;
        }
    }
    return !payload.isEmpty();
}

bool decodeBcd_02(const QByteArray &payload, double &value)
{
    if (payload.size() < 2 || isAllFF(payload.left(2))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    value = loOctet(b0) * 10.0 +
            hiOctet(b1) * 1.0 +
            loOctet(b1) * 0.1;
    return true;
}

bool decodeBcd_10_17(const QByteArray &payload, double &value)
{
    if (payload.size() < 2 || isAllFF(payload.left(2))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    value = hiOctet(b0) * 100.0 +
            loOctet(b0) * 10.0 +
            hiOctet(b1) * 1.0 +
            loOctet(b1) * 0.1;
    return true;
}

bool decodeBcd_09_0F_391A(const QByteArray &payload, double &value)
{
    if (payload.size() < 3 || isAllFF(payload.left(3))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    const quint8 b2 = static_cast<quint8>(payload.at(2));
    value = loOctet(b0) * 100.0 +
            hiOctet(b1) * 10.0 +
            loOctet(b1) * 1.0 +
            hiOctet(b2) * 0.1 +
            loOctet(b2) * 0.01;
    return true;
}

bool decodeBcd_1A_1F_20(const QByteArray &payload, double &value)
{
    if (payload.size() < 3 || isAllFF(payload.left(3))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    const quint8 b2 = static_cast<quint8>(payload.at(2));
    value = loOctet(b0) * 1000.0 +
            hiOctet(b1) * 100.0 +
            loOctet(b1) * 10.0 +
            hiOctet(b2) * 1.0 +
            loOctet(b2) * 0.1;
    return true;
}

bool decodeBcd_26(const QByteArray &payload, double &value)
{
    if (payload.size() < 3 || isAllFF(payload.left(3))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    const quint8 b2 = static_cast<quint8>(payload.at(2));
    value = hiOctet(b0) * 10000.0 +
            loOctet(b0) * 1000.0 +
            hiOctet(b1) * 100.0 +
            loOctet(b1) * 10.0 +
            hiOctet(b2) * 1.0 +
            loOctet(b2) * 0.1;
    return true;
}

bool decodeBcd_27(const QByteArray &payload, double &value)
{
    if (payload.size() < 5 || isAllFF(payload.left(5))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    const quint8 b2 = static_cast<quint8>(payload.at(2));
    const quint8 b3 = static_cast<quint8>(payload.at(3));
    const quint8 b4 = static_cast<quint8>(payload.at(4));
    value = loOctet(b0) * 100000.0 +
            hiOctet(b1) * 10000.0 +
            loOctet(b1) * 1000.0 +
            hiOctet(b2) * 100.0 +
            loOctet(b2) * 10.0 +
            hiOctet(b3) * 1.0 +
            loOctet(b3) * 0.1 +
            hiOctet(b4) * 0.01 +
            loOctet(b4) * 0.001;
    return true;
}

bool decodeBcd_36(const QByteArray &payload, double &value)
{
    if (payload.size() < 3 || isAllFF(payload.left(3))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    const quint8 b2 = static_cast<quint8>(payload.at(2));
    value = hiOctet(b0) * 100.0 +
            loOctet(b0) * 10.0 +
            hiOctet(b1) * 1.0 +
            loOctet(b1) * 0.1 +
            hiOctet(b2) * 0.01 +
            loOctet(b2) * 0.001;
    return true;
}

bool decodeBcd_38(const QByteArray &payload, double &value)
{
    if (payload.size() < 2 || isAllFF(payload.left(2))) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(payload.at(0));
    const quint8 b1 = static_cast<quint8>(payload.at(1));
    value = hiOctet(b0) * 10.0 +
            loOctet(b0) * 1.0 +
            hiOctet(b1) * 0.1 +
            loOctet(b1) * 0.01;
    return true;
}

quint8 queryItemLengthTag(int itemCode)
{
    switch (itemCode) {
    case 0x20:
    case 0x1A:
    case 0x1F:
    case 0x26:
        return 0x19;
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
        return 0x23;
    case 0x27:
        return 0x2B;
    case 0x36:
        return 0x1B;
    case 0x38:
        return 0x12;
    case 0x68:
        return 0x2A;
    case 0x09:
    case 0x03:
        return 0x1A;
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        return 0x11;
    case 0xF3:
        return 0xF3;
    default:
        return 0x00;
    }
}

int expectedValueLength(int itemCode, quint8 formatTag)
{
    Q_UNUSED(itemCode);
    switch (formatTag) {
    case 0x11:
    case 0x12:
        return 2;
    case 0x19:
    case 0x1A:
    case 0x1B:
        return 3;
    case 0x23:
        return 4;
    case 0x2A:
    case 0x2B:
        return 5;
    case 0xF3:
        return -1;
    default:
        return -1;
    }
}

QString decodeSampleTimeText(const QByteArray &payload)
{
    const int pos = payload.indexOf(QByteArray::fromHex("F0F0"));
    if (pos < 0 || pos + 7 > payload.size()) {
        return {};
    }

    const auto bcd = [](quint8 value) -> int {
        return BCD2Hex(value);
    };

    const quint8 year = static_cast<quint8>(payload.at(pos + 2));
    const quint8 month = static_cast<quint8>(payload.at(pos + 3));
    const quint8 day = static_cast<quint8>(payload.at(pos + 4));
    const quint8 hour = static_cast<quint8>(payload.at(pos + 5));
    const quint8 minute = static_cast<quint8>(payload.at(pos + 6));
    return QStringLiteral("20%1-%2-%3 %4:%5")
        .arg(bcd(year), 2, 10, QLatin1Char('0'))
        .arg(bcd(month), 2, 10, QLatin1Char('0'))
        .arg(bcd(day), 2, 10, QLatin1Char('0'))
        .arg(bcd(hour), 2, 10, QLatin1Char('0'))
        .arg(bcd(minute), 2, 10, QLatin1Char('0'));
}

bool locateRealtimeItem(const QByteArray &payload, int fallbackItem, int &itemCode, quint8 &formatTag, QByteArray &valueBytes)
{
    itemCode = -1;
    formatTag = 0x00;
    valueBytes.clear();

    for (int i = payload.size() - 2; i >= 0; --i) {
        const int code = static_cast<quint8>(payload.at(i));
        const quint8 tag = static_cast<quint8>(payload.at(i + 1));
        const quint8 expectedTag = queryItemLengthTag(code);
        if (expectedTag == 0x00 || expectedTag != tag) {
            continue;
        }

        const int valueLen = expectedValueLength(code, tag);
        if (valueLen < 0) {
            itemCode = code;
            formatTag = tag;
            valueBytes = payload.mid(i + 2);
            return true;
        }

        if (i + 2 + valueLen <= payload.size()) {
            itemCode = code;
            formatTag = tag;
            valueBytes = payload.mid(i + 2, valueLen);
            return true;
        }
    }

    if (fallbackItem > 0) {
        const quint8 fallbackTag = queryItemLengthTag(fallbackItem);
        const int valueLen = expectedValueLength(fallbackItem, fallbackTag);
        if (fallbackTag != 0x00 && valueLen > 0 && payload.size() >= valueLen) {
            itemCode = fallbackItem;
            formatTag = fallbackTag;
            valueBytes = payload.right(valueLen);
            return true;
        }
    }

    return false;
}

QByteArray extractF3ImagePayload(const QByteArray &payload)
{
    if (payload.isEmpty()) {
        return {};
    }

    const int pos = payload.indexOf(QByteArray::fromHex("F3F3"));
    if (pos < 0) {
        return {};
    }

    return payload.mid(pos + 2);
}

QString unitTextForItem(int itemCode)
{
    switch (itemCode) {
    case 0x02:
    case 0x03:
    case 0x35:
        return QStringLiteral("°C");
    case 0x08:
        return QStringLiteral("hPa");
    case 0x09:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 160:
    case 161:
    case 162:
    case 163:
    case 164:
    case 165:
    case 166:
    case 167:
    case 168:
        return QStringLiteral("m");
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 24:
    case 170:
        return QStringLiteral("%");
    case 0x1A:
    case 0x1F:
    case 0x20:
    case 0x26:
        return QStringLiteral("mm");
    case 0x27:
    case 0x36:
        return QStringLiteral("m3/s");
    case 0x38:
        return QStringLiteral("V");
    case 0x68:
    case 118:
        return QStringLiteral("m3");
    case 55:
        return QStringLiteral("m/s");
    case 70:
        return QStringLiteral("pH");
    case 71:
        return QStringLiteral("mg/L");
    default:
        return {};
    }
}

QString formatBusinessValue(double value, int itemCode)
{
    int decimals = 2;
    switch (itemCode) {
    case 0x38:
    case 0x03:
    case 0x35:
        decimals = 2;
        break;
    case 0x27:
    case 0x36:
    case 0x68:
        decimals = 3;
        break;
    default:
        decimals = 2;
        break;
    }

    QString text = QString::number(value, 'f', decimals);
    while (text.contains('.') && text.endsWith('0')) {
        text.chop(1);
    }
    if (text.endsWith('.')) {
        text.chop(1);
    }
    return text;
}

DecodedRealtimeData decodeRealtimePayload(const QByteArray &payload, const SensorRecord *record)
{
    DecodedRealtimeData result;
    if (payload.isEmpty()) {
        result.statusText = zh(u8"\u8bbe\u5907\u8fd4\u56de\u7a7a\u6570\u636e");
        return result;
    }

    const int fallbackItem = record != nullptr ? record->elementCode : -1;
    result.sampleTimeText = decodeSampleTimeText(payload);

    const QByteArray f3ImagePayload = extractF3ImagePayload(payload);
    if (!f3ImagePayload.isEmpty()) {
        result.isImage = true;
        result.ok = true;
        result.imagePayload = f3ImagePayload;
        result.statusText = zh(u8"\u5df2\u68c0\u6d4b\u5230 F3 \u56fe\u7247\u6570\u636e");
        return result;
    }

    int itemCode = -1;
    quint8 formatTag = 0x00;
    QByteArray valueBytes;
    if (!locateRealtimeItem(payload, fallbackItem, itemCode, formatTag, valueBytes)) {
        result.statusText = zh(u8"\u672a\u627e\u5230\u53ef\u89e3\u6790\u7684\u8981\u7d20\u6570\u636e\u6bb5");
        return result;
    }

    if (itemCode == 0xF3 || fallbackItem == 0xF3) {
        result.isImage = true;
        result.ok = !valueBytes.isEmpty();
        result.imagePayload = valueBytes;
        result.statusText = result.ok
                                ? zh(u8"\u5df2\u8fd4\u56de\u56fe\u7247\u6570\u636e")
                                : zh(u8"\u56fe\u7247\u6570\u636e\u4e3a\u7a7a");
        return result;
    }

    if (valueBytes.isEmpty()) {
        result.statusText = zh(u8"\u8fd4\u56de\u8f7d\u8377\u4e0d\u542b\u6709\u6548\u6570\u503c");
        return result;
    }

    if (isAllFF(valueBytes)) {
        result.ok = true;
        result.statusText = zh(u8"\u5f53\u524d\u65e0\u6709\u6548\u5b9e\u65f6\u6570\u636e");
        result.valueText = zh(u8"--");
        result.unitText = unitTextForItem(itemCode);
        return result;
    }

    double numericValue = 0.0;
    bool decoded = false;
    switch (itemCode) {
    case 0x02:
        decoded = decodeBcd_02(valueBytes, numericValue);
        break;
    case 0x03:
        if (formatTag == 0x10) {
            decoded = decodeBcd_10_17(valueBytes, numericValue);
        } else {
            decoded = decodeBcd_09_0F_391A(valueBytes, numericValue);
        }
        break;
    case 0x09:
        decoded = decodeBcd_09_0F_391A(valueBytes, numericValue);
        break;
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        decoded = decodeBcd_10_17(valueBytes, numericValue);
        break;
    case 0x1A:
    case 0x1F:
    case 0x20:
        decoded = decodeBcd_1A_1F_20(valueBytes, numericValue);
        break;
    case 0x26:
        decoded = decodeBcd_26(valueBytes, numericValue);
        break;
    case 0x27:
    case 0x68:
        decoded = decodeBcd_27(valueBytes, numericValue);
        break;
    case 0x36:
        decoded = decodeBcd_36(valueBytes, numericValue);
        break;
    case 0x38:
        decoded = decodeBcd_38(valueBytes, numericValue);
        break;
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
        if (formatTag == 0x1A) {
            decoded = decodeBcd_09_0F_391A(valueBytes, numericValue);
        } else {
            decoded = decodeBcd_36(valueBytes, numericValue);
        }
        break;
    default:
        break;
    }

    if (decoded) {
        result.ok = true;
        result.valueText = formatBusinessValue(numericValue, itemCode);
        result.unitText = unitTextForItem(itemCode);
        result.statusText = zh(u8"\u5df2\u8fd4\u56de\u5b9e\u65f6\u6570\u636e");
        return result;
    }

    result.statusText = zh(u8"\u5f53\u524d\u8981\u7d20\u8fd4\u56de\u6210\u529f\uff0c\u4f46\u672c\u9875\u6682\u672a\u5b8c\u6210\u89e3\u6790");
    return result;
}

QWidget *createSectionHeader(const QString &title, QWidget *parent = nullptr)
{
    auto *container = new QFrame(parent);
    container->setStyleSheet(QStringLiteral(
        "QFrame{background:transparent;border:none;}"
        "QLabel{font-size:16px;font-weight:700;color:#0f172a;}"));

    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *accent = new QFrame(container);
    accent->setFixedSize(4, 18);
    accent->setStyleSheet(QStringLiteral("background:#2f78e8;border-radius:2px;"));

    auto *label = new QLabel(title, container);
    layout->addWidget(accent, 0, Qt::AlignVCenter);
    layout->addWidget(label, 0, Qt::AlignVCenter);
    layout->addStretch();
    return container;
}

QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:#2f78e8;color:#ffffff;border:none;border-radius:6px;padding:0 18px;font-weight:600;min-height:40px;}"
        "QPushButton:hover{background:#2468cc;}"
        "QPushButton:pressed{background:#1d57aa;}"
        "QPushButton:disabled{background:#b8c7db;color:#eef2f7;}");
}

QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{background:#ffffff;color:#2f78e8;border:1px solid #bfd4f6;border-radius:6px;padding:0 18px;font-weight:600;min-height:40px;}"
        "QPushButton:hover{background:#edf4ff;}"
        "QPushButton:pressed{background:#dbe9ff;}"
        "QPushButton:disabled{background:#f8fafc;color:#b8c7db;border:1px solid #dde6f2;}");
}

}

DataQueryPage::DataQueryPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    m_imageRetryTimer = new QTimer(this);
    m_imageRetryTimer->setSingleShot(true);
    refreshSensorOptions();
    showEmptyState(zh(u8"\u6682\u65e0\u7ed3\u679c"), zh(u8"\u9009\u62e9\u4f20\u611f\u5668\u5e76\u67e5\u770b\u5b9e\u65f6\u8fd4\u56de\u6570\u636e\u3002"));
}

void DataQueryPage::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(14);

    setStyleSheet(QStringLiteral("DataQueryPage{background:transparent;}"));

    auto *queryCard = new QFrame(this);
    queryCard->setFrameShape(QFrame::StyledPanel);
    queryCard->setStyleSheet(QStringLiteral(
        "QFrame{background:#ffffff;border:1px solid #d8e2f0;border-radius:10px;}"
        "QLabel{color:#1f2937;font-size:14px;background:transparent;border:none;border-radius:0;}"
        "QLabel#fieldLabel{color:#334e68;font-size:13px;font-weight:700;border:none;background:transparent;padding:0;}"
        "QComboBox{min-height:40px;border:1px solid #cbd5e1;border-radius:6px;padding:0 40px 0 12px;background:#ffffff;color:#243b53;font-size:14px;}"
        "QComboBox:hover{border:1px solid #98b2cc;}"
        "QComboBox:focus{border:1px solid #2f78e8;}"
        "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:32px;border:none;}"
        "QComboBox::down-arrow{width:0px;height:0px;border-left:6px solid transparent;border-right:6px solid transparent;border-top:7px solid #64748b;margin-right:10px;}"
        "QPushButton{min-width:128px;min-height:40px;max-height:40px;background:#2f78e8;color:#ffffff;border:none;border-radius:6px;padding:0 18px;font-size:14px;font-weight:600;}"
        "QPushButton:hover{background:#2468cc;}"
        "QPushButton:pressed{background:#1d57aa;}"
        "QPushButton:disabled{background:#b8c7db;color:#eef2f7;}"));
    auto *queryCardLayout = new QVBoxLayout(queryCard);
    queryCardLayout->setContentsMargins(18, 16, 18, 16);
    queryCardLayout->setSpacing(0);

    auto *queryContent = new QWidget(queryCard);
    auto *queryLayout = new QHBoxLayout(queryContent);
    queryLayout->setContentsMargins(0, 0, 0, 0);
    queryLayout->setSpacing(10);

    auto *sensorLabel = new QLabel(zh(u8"\u4f20\u611f\u5668"), queryContent);
    sensorLabel->setObjectName(QStringLiteral("fieldLabel"));
    sensorLabel->setFixedWidth(44);
    sensorLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_sensorCombo = new QComboBox(queryContent);
    m_sensorCombo->setMinimumWidth(320);
    m_sensorCombo->setMaximumWidth(460);
    m_sensorCombo->setMinimumHeight(40);
    m_sensorCombo->setMaximumHeight(40);
    m_sensorCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_refreshButton = new QPushButton(zh(u8"\u83b7\u53d6\u4f20\u611f\u5668"), queryContent);
    m_refreshButton->setStyleSheet(secondaryButtonStyle());
    m_refreshButton->setMinimumSize(136, 40);
    m_refreshButton->setMaximumSize(136, 40);
    connect(m_refreshButton, &QPushButton::clicked, this, &DataQueryPage::requestSensorParamRead);

    m_queryButton = new QPushButton(zh(u8"\u67e5\u8be2\u6570\u636e"), queryContent);
    m_queryButton->setStyleSheet(primaryButtonStyle());
    m_queryButton->setMinimumSize(136, 40);
    m_queryButton->setMaximumSize(136, 40);
    connect(m_queryButton, &QPushButton::clicked, this, &DataQueryPage::requestRealtimeData);

    queryLayout->addWidget(sensorLabel, 0, Qt::AlignVCenter);
    queryLayout->addWidget(m_sensorCombo, 0, Qt::AlignVCenter);
    queryLayout->addWidget(m_refreshButton, 0, Qt::AlignVCenter);
    queryLayout->addWidget(m_queryButton, 0, Qt::AlignVCenter);
    queryLayout->addStretch(1);

    queryCardLayout->addWidget(queryContent);
    rootLayout->addWidget(queryCard);

    auto *resultCard = new QFrame(this);
    resultCard->setFrameShape(QFrame::StyledPanel);
    resultCard->setStyleSheet(QStringLiteral(
        "QFrame{background:#ffffff;border:1px solid #d8e2f0;border-radius:10px;}"
        "QLabel{color:#111827;background:transparent;}"
        "QTableWidget{border:1px solid #e5edf6;background:#ffffff;gridline-color:#edf2f7;color:#344054;font-size:13px;border-radius:6px;}"
        "QHeaderView::section{background:#f8fafc;color:#486581;border:none;border-bottom:1px solid #e6edf6;padding:10px 12px;font-weight:700;}"
        "QTableWidget::item{padding:10px 12px;border-bottom:1px solid #f2f4f7;}"));
    auto *resultCardLayout = new QVBoxLayout(resultCard);
    resultCardLayout->setContentsMargins(18, 16, 18, 18);
    resultCardLayout->setSpacing(12);
    resultCardLayout->addWidget(createSectionHeader(zh(u8"\u67e5\u8be2\u7ed3\u679c"), resultCard));

    m_statusLabel = new QLabel(zh(u8"\u8bf7\u9009\u62e9\u4f20\u611f\u5668\u540e\u6267\u884c\u67e5\u8be2\u3002"), resultCard);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color:#475467;font-size:13px;padding:10px 12px;"
        "background:#f8fafc;border:1px solid #e5edf6;border-radius:6px;"));
    resultCardLayout->addWidget(m_statusLabel);

    m_resultStack = new QStackedWidget(resultCard);
    m_resultStack->setStyleSheet(QStringLiteral("QStackedWidget{background:#ffffff;border:none;}"));

    m_emptyStatePage = new QWidget(m_resultStack);
    auto *emptyLayout = new QVBoxLayout(m_emptyStatePage);
    emptyLayout->setContentsMargins(24, 30, 24, 30);
    emptyLayout->setSpacing(12);
    emptyLayout->addStretch();

    auto *emptyIcon = new QFrame(m_emptyStatePage);
    emptyIcon->setFixedSize(84, 84);
    emptyIcon->setStyleSheet(QStringLiteral(
        "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #f8fbff,stop:1 #e4ecf8);"
        "border:1px solid #dce5f0;border-radius:20px;}"));
    emptyLayout->addWidget(emptyIcon, 0, Qt::AlignHCenter);

    m_emptyTitleLabel = new QLabel(zh(u8"\u6682\u65e0\u7ed3\u679c"), m_emptyStatePage);
    m_emptyTitleLabel->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;color:#243b53;"));
    emptyLayout->addWidget(m_emptyTitleLabel, 0, Qt::AlignHCenter);

    m_emptyHintLabel = new QLabel(zh(u8"\u9009\u62e9\u4f20\u611f\u5668\u5e76\u67e5\u770b\u5b9e\u65f6\u8fd4\u56de\u6570\u636e\u3002"), m_emptyStatePage);
    m_emptyHintLabel->setWordWrap(true);
    m_emptyHintLabel->setAlignment(Qt::AlignHCenter);
    m_emptyHintLabel->setStyleSheet(QStringLiteral("font-size:13px;color:#829ab1;"));
    m_emptyHintLabel->setMaximumWidth(260);
    emptyLayout->addWidget(m_emptyHintLabel, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    m_resultStack->addWidget(m_emptyStatePage);

    m_resultContentPage = new QWidget(m_resultStack);
    auto *contentLayout = new QVBoxLayout(m_resultContentPage);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_resultTable = new QTableWidget(m_resultContentPage);
    m_resultTable->setColumnCount(2);
    m_resultTable->setHorizontalHeaderLabels(QStringList()
                                             << zh(u8"\u5b57\u6bb5")
                                             << zh(u8"\u6570\u503c"));
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_resultTable->setFocusPolicy(Qt::NoFocus);
    m_resultTable->setAlternatingRowColors(false);
    m_resultTable->setShowGrid(false);
    m_resultTable->setMinimumHeight(360);
    m_resultTable->setFrameShape(QFrame::NoFrame);
    m_resultTable->verticalHeader()->setDefaultSectionSize(42);
    contentLayout->addWidget(m_resultTable);

    m_imageCard = new QFrame(m_resultContentPage);
    m_imageCard->setStyleSheet(QStringLiteral(
        "QFrame{background:#ffffff;border:none;border-radius:0;}"
        "QLabel{border:none;background:transparent;}"
        "QLabel#imageMeta{font-size:12px;color:#667085;}"));
    auto *imageLayout = new QVBoxLayout(m_imageCard);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(8);

    m_imageMetaLabel = new QLabel(zh(u8"\u5f53\u524d\u8fd4\u56de\u6570\u636e\u4e0d\u542b\u56fe\u7247\u3002"), m_imageCard);
    m_imageMetaLabel->setObjectName(QStringLiteral("imageMeta"));
    imageLayout->addWidget(m_imageMetaLabel);

    m_imageScrollArea = new QScrollArea(m_imageCard);
    m_imageScrollArea->setWidgetResizable(false);
    m_imageScrollArea->setFrameShape(QFrame::NoFrame);
    m_imageScrollArea->setMinimumHeight(280);
    m_imageScrollArea->setStyleSheet(QStringLiteral("QScrollArea{background:#ffffff;border:1px dashed #c7d2e0;border-radius:6px;}"));

    auto *imageContainer = new QWidget(m_imageScrollArea);
    auto *imageContainerLayout = new QVBoxLayout(imageContainer);
    imageContainerLayout->setContentsMargins(20, 20, 20, 20);
    imageContainerLayout->setSpacing(0);

    m_imagePreviewLabel = new QLabel(imageContainer);
    m_imagePreviewLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_imagePreviewLabel->setMinimumSize(240, 180);
    m_imagePreviewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_imagePreviewLabel->setText(zh(u8"\u6682\u65e0\u56fe\u7247"));
    m_imagePreviewLabel->setStyleSheet(QStringLiteral("QLabel{color:#98a2b3;font-size:13px;background:#ffffff;}"));
    imageContainerLayout->addWidget(m_imagePreviewLabel);

    m_imageScrollArea->setWidget(imageContainer);
    imageLayout->addWidget(m_imageScrollArea);
    contentLayout->addWidget(m_imageCard);
    m_imageCard->hide();

    m_resultStack->addWidget(m_resultContentPage);
    resultCardLayout->addWidget(m_resultStack);

    rootLayout->addWidget(resultCard, 1);
}

void DataQueryPage::appendResultRow(const QString &field, const QString &value)
{
    const int row = m_resultTable->rowCount();
    m_resultTable->insertRow(row);
    m_resultTable->setItem(row, 0, new QTableWidgetItem(field));
    auto *valueItem = new QTableWidgetItem(value);
    valueItem->setToolTip(value);
    m_resultTable->setItem(row, 1, valueItem);
}

void DataQueryPage::clearResults()
{
    m_resultTable->setRowCount(0);
    m_resultTable->show();
    m_imagePreviewLabel->clear();
    m_imagePreviewLabel->setText(zh(u8"\u6682\u65e0\u56fe\u7247"));
    m_imageMetaLabel->setText(zh(u8"\u5f53\u524d\u8fd4\u56de\u6570\u636e\u4e0d\u542b\u56fe\u7247\u3002"));
    m_imageCard->hide();
}

void DataQueryPage::setStatusText(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError
                                     ? QStringLiteral("color:#b42318;font-size:13px;padding:10px 12px;background:#fef3f2;border:1px solid #fecdca;border-radius:6px;")
                                     : QStringLiteral("color:#475467;font-size:13px;padding:10px 12px;background:#f8fafc;border:1px solid #e5edf6;border-radius:6px;"));
}

void DataQueryPage::showEmptyState(const QString &title, const QString &hint)
{
    clearResults();
    m_emptyTitleLabel->setText(title);
    m_emptyHintLabel->setText(hint);
    m_resultStack->setCurrentWidget(m_emptyStatePage);
}

void DataQueryPage::showResultContent()
{
    m_resultStack->setCurrentWidget(m_resultContentPage);
}

void DataQueryPage::updateImagePreview(const QByteArray &payload)
{
    const QByteArray normalizedPayload = normalizeJpegPayload(payload);
    QBuffer buffer;
    buffer.setData(normalizedPayload);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader;
    reader.setDecideFormatFromContent(true);
    reader.setDevice(&buffer);
    const QImage image = reader.read();
    if (image.isNull()) {
        m_imageCard->hide();
        m_resultTable->show();
        m_imagePreviewLabel->clear();
        m_imagePreviewLabel->setText(zh(u8"\u6682\u65e0\u56fe\u7247"));
        m_imageMetaLabel->setText(zh(u8"\u5f53\u524d\u8fd4\u56de\u6570\u636e\u4e0d\u662f\u53ef\u89e3\u6790\u7684\u56fe\u7247\u3002"));
        return;
    }

    m_resultTable->hide();
    m_imageCard->show();
    const QPixmap pixmap = QPixmap::fromImage(image);
    m_imagePreviewLabel->setPixmap(pixmap);
    m_imagePreviewLabel->resize(pixmap.size());
    m_imagePreviewLabel->setMinimumSize(pixmap.size());
    m_imageMetaLabel->setText(zh(u8"\u5df2\u68c0\u6d4b\u5230\u56fe\u7247\u6570\u636e\uff1a%1 x %2\uff0c%3 \u5b57\u8282\uff08\u6309 1:1 \u539f\u56fe\u663e\u793a\uff09")
                                  .arg(image.width())
                                  .arg(image.height())
                                  .arg(normalizedPayload.size()));
}

void DataQueryPage::refreshSensorOptions()
{
    const SensorRecord *selectedSensor = currentSensor();
    const QString previousId = selectedSensor != nullptr ? selectedSensor->id : QString();
    m_sensorOptions.clear();
    m_sensorCombo->clear();

    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_sensorPage == nullptr) {
        setStatusText(zh(u8"\u4f20\u611f\u5668\u9875\u672a\u521d\u59cb\u5316\uff0c\u6682\u65f6\u65e0\u6cd5\u52a0\u8f7d\u67e5\u8be2\u5bf9\u8c61\u3002"), true);
        showEmptyState(zh(u8"\u6682\u65e0\u7ed3\u679c"), zh(u8"\u4f20\u611f\u5668\u672a\u5c31\u7eea"));
        return;
    }

    const QVector<SensorRecord> records = m_RTU_ParameterSetting->m_sensorPage->sensorRecords();
    for (const SensorRecord &record : records) {
        SensorOption option;
        option.record = record;
        option.displayText = !record.name.trimmed().isEmpty()
                                 ? record.name.trimmed()
                                 : (!record.element.trimmed().isEmpty()
                                        ? record.element.trimmed()
                                        : QStringLiteral("%1(%2)").arg(record.model, record.id));
        m_sensorOptions.push_back(option);
        m_sensorCombo->addItem(option.displayText);
    }

    {
        SensorOption batteryOption;
        batteryOption.record = makeBatteryVoltageRecord();
        batteryOption.displayText = batteryOption.record.name;
        m_sensorOptions.push_back(batteryOption);
        m_sensorCombo->addItem(batteryOption.displayText);
    }

    for (int i = 0; i < m_sensorOptions.size(); ++i) {
        if (m_sensorOptions.at(i).record.id == previousId) {
            m_sensorCombo->setCurrentIndex(i);
            break;
        }
    }

    if (records.isEmpty()) {
        setStatusText(zh(u8"\u6682\u65e0\u4f20\u611f\u5668\u914d\u7f6e"), true);
        showEmptyState(zh(u8"\u6682\u65e0\u7ed3\u679c"), zh(u8"\u53ef\u76f4\u63a5\u67e5\u8be2\u84c4\u7535\u6c60\u7535\u538b\uff0c\u6216\u5148\u914d\u7f6e\u4f20\u611f\u5668"));
    } else {
        setStatusText(zh(u8"\u5df2\u52a0\u8f7d %1 \u4e2a\u4f20\u611f\u5668\uff0c\u5e76\u9644\u52a0\u84c4\u7535\u6c60\u7535\u538b\u67e5\u8be2")
                          .arg(records.size()));
    }
}

const SensorRecord *DataQueryPage::currentSensor() const
{
    const int index = m_sensorCombo->currentIndex();
    if (index < 0 || index >= m_sensorOptions.size()) {
        return nullptr;
    }
    return &m_sensorOptions.at(index).record;
}

void DataQueryPage::renderSensorInfo(const SensorRecord &record)
{
    if (!record.id.isEmpty() && !isVirtualBatteryRecord(record)) {
        appendResultRow(zh(u8"\u4f20\u611f\u5668\u6807\u8bc6"), record.id);
    }
    appendResultRow(zh(u8"\u540d\u79f0"), !record.name.isEmpty() ? record.name : record.id);
    appendResultRow(zh(u8"\u91c7\u96c6\u8981\u7d20"), record.element.isEmpty() ? record.name : record.element);
    if (!record.model.isEmpty()) {
        appendResultRow(zh(u8"\u4f20\u611f\u5668\u578b\u53f7"), record.model);
    }
    if (!record.port.isEmpty()) {
        appendResultRow(zh(u8"\u63a5\u5165\u7aef\u53e3"), record.port);
    }
    if (!record.address485.isEmpty()) {
        appendResultRow(zh(u8"485 \u5730\u5740"), record.address485);
    }
}

void DataQueryPage::requestSensorParamRead()
{
    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_Device_connection == nullptr) {
        setStatusText(zh(u8"\u901a\u4fe1\u529f\u80fd\u672a\u521d\u59cb\u5316\uff0c\u65e0\u6cd5\u83b7\u53d6\u4f20\u611f\u5668\u53c2\u6570\u3002"), true);
        return;
    }

    uint32_t pos = 0;
    m_RTU_ParameterSetting->isbasedate = false;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage), AFN_READBASEPARAM, STX, 8, MTYPE_DOWN);
    pos += 22;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    const uint16_t crcValue = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte, pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos + 1] = (crcValue & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crcValue >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();

    setStatusText(zh(u8"\u5df2\u53d1\u9001\u83b7\u53d6\u4f20\u611f\u5668\u53c2\u6570\u547d\u4ee4\u3002"));
}

void DataQueryPage::requestRealtimeData()
{
    const SensorRecord *record = currentSensor();
    if (record == nullptr) {
        clearResults();
        setStatusText(zh(u8"\u8bf7\u5148\u83b7\u53d6\u5e76\u9009\u62e9\u4f20\u611f\u5668"), true);
        showEmptyState(zh(u8"\u6682\u65e0\u7ed3\u679c"), zh(u8"\u8bf7\u5148\u83b7\u53d6\u4f20\u611f\u5668\u53c2\u6570"));
        return;
    }

    clearResults();
    showResultContent();
    renderSensorInfo(*record);
    appendResultRow(zh(u8"\u67e5\u8be2\u52a8\u4f5c"), zh(u8"\u53ec\u6d4b\u4f20\u611f\u5668"));
    appendResultRow(zh(u8"\u72b6\u6001"), zh(u8"\u7b49\u5f85\u8fd4\u56de"));
    setStatusText(zh(u8"\u6b63\u5728\u6309\u9009\u4e2d\u4f20\u611f\u5668\u67e5\u8be2\u5b9e\u65f6\u6570\u636e"));

    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_runtimePage == nullptr) {
        setStatusText(zh(u8"\u8fd0\u884c\u53c2\u6570\u9875\u672a\u521d\u59cb\u5316\uff0c\u65e0\u6cd5\u53d1\u9001\u53ec\u6d4b\u547d\u4ee4\u3002"), true);
        return;
    }

    if (record->elementCode == 0xF3) {
        resetImageAssembly();
    }

    sendSpecifiedRealtimeQuery(*record);
}

void DataQueryPage::sendSpecifiedRealtimeQuery(const SensorRecord &record)
{
    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_Device_connection == nullptr) {
        setStatusText(zh(u8"\u901a\u4fe1\u529f\u80fd\u672a\u521d\u59cb\u5316\uff0c\u65e0\u6cd5\u53d1\u9001\u67e5\u8be2\u547d\u4ee4"), true);
        return;
    }

    const int itemCode = record.elementCode;
    const quint8 itemTag = queryItemLengthTag(itemCode);
    if (itemCode <= 0 || itemTag == 0x00) {
        clearResults();
        renderSensorInfo(record);
        appendResultRow(zh(u8"\u72b6\u6001"), zh(u8"\u5f53\u524d\u4f20\u611f\u5668\u8981\u7d20\u6682\u4e0d\u652f\u6301\u76f4\u63a5\u53ec\u6d4b"));
        setStatusText(zh(u8"\u8981\u7d20 %1 \u6682\u4e0d\u652f\u6301\u6309 CommServer \u89c4\u5219\u67e5\u8be2")
                          .arg(record.element.isEmpty() ? record.name : record.element),
                      true);
        return;
    }

    const bool isImageQuery = itemCode == 0xF3;
    if (isImageQuery) {
        appendResultRow(zh(u8"查询类型"), zh(u8"图片/特殊要素查询"));
        appendResultRow(zh(u8"协议 AFN"), QStringLiteral("0x36"));
        appendResultRow(zh(u8"发送方式"), zh(u8"发送标准 AFN 0x36 空载荷查询帧"));
        setStatusText(zh(u8"正在发送 AFN 0x36 图片查询报文，等待设备返回图片分包"));
        m_pendingImageRecord = record;
        m_waitingImageResponse = true;
        m_imageFallbackAttempted = true;
        sendImageQueryFrame(record, false);
        return;
    }

    const quint8 afn = 0x3A;
    QByteArray payload;
    payload.append(static_cast<char>(0x63));
    payload.append(static_cast<char>(0x09));
    payload.append(static_cast<char>(0x01));
    payload.append(static_cast<char>(itemCode & 0xff));
    payload.append(static_cast<char>(itemTag));

    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),
                           afn,
                           STX,
                           static_cast<int16_t>(payload.size() + 8),
                           MTYPE_DOWN);
    pos += 22;
    if (!payload.isEmpty()) {
        memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte + pos, payload.constData(), payload.size());
        pos += static_cast<uint32_t>(payload.size());
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

void DataQueryPage::sendImageQueryFrame(const SensorRecord &record, bool withCameraExtension)
{
    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_Device_connection == nullptr) {
        return;
    }

    Q_UNUSED(record);
    Q_UNUSED(withCameraExtension);

    QByteArray payload;

    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),
                           0x36,
                           STX,
                           static_cast<int16_t>(8 + payload.size()),
                           MTYPE_DOWN);
    pos += 22;
    if (!payload.isEmpty()) {
        memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte + pos, payload.constData(), payload.size());
        pos += static_cast<uint32_t>(payload.size());
    }
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    const uint16_t crcValue = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte, pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos + 1] = (crcValue & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crcValue >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();

    appendResultRow(zh(u8"报文体"), zh(u8"空载荷"));
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}

QByteArray DataQueryPage::currentPayload() const
{
    if (m_RTU_ParameterSetting == nullptr) {
        return {};
    }

    const uint16_t dataLen =
        (static_cast<uint16_t>(m_RTU_ParameterSetting->m_recivemessage.messageByte[11]) << 8) |
        static_cast<uint16_t>(m_RTU_ParameterSetting->m_recivemessage.messageByte[12]);
    const uint16_t totalLen = dataLen + 15;
    if (totalLen < 23 || m_RTU_ParameterSetting->rMessageLen < totalLen) {
        return {};
    }

    return QByteArray(reinterpret_cast<const char *>(m_RTU_ParameterSetting->m_recivemessage.messageByte + 22),
                      totalLen - 23);
}

QByteArray DataQueryPage::currentFrameBytes() const
{
    if (m_RTU_ParameterSetting == nullptr) {
        return {};
    }

    const uint16_t dataLen =
        (static_cast<uint16_t>(m_RTU_ParameterSetting->m_recivemessage.messageByte[11]) << 8) |
        static_cast<uint16_t>(m_RTU_ParameterSetting->m_recivemessage.messageByte[12]);
    const uint16_t totalLen = dataLen + 15;
    if (totalLen < 23 || m_RTU_ParameterSetting->rMessageLen < totalLen) {
        return {};
    }

    return QByteArray(reinterpret_cast<const char *>(m_RTU_ParameterSetting->m_recivemessage.messageByte), totalLen);
}

QString DataQueryPage::formatMessageTime() const
{
    if (m_RTU_ParameterSetting == nullptr) {
        return {};
    }

    const auto bcd = [](uint8_t value) -> int {
        return BCD2Hex(value);
    };

    const auto &message = m_RTU_ParameterSetting->m_recivemessage.message;
    return QStringLiteral("20%1-%2-%3 %4:%5:%6")
        .arg(bcd(message.Messagetime[0]), 2, 10, QLatin1Char('0'))
        .arg(bcd(message.Messagetime[1]), 2, 10, QLatin1Char('0'))
        .arg(bcd(message.Messagetime[2]), 2, 10, QLatin1Char('0'))
        .arg(bcd(message.Messagetime[3]), 2, 10, QLatin1Char('0'))
        .arg(bcd(message.Messagetime[4]), 2, 10, QLatin1Char('0'))
        .arg(bcd(message.Messagetime[5]), 2, 10, QLatin1Char('0'));
}

QString DataQueryPage::extractPrintableText(const QByteArray &payload) const
{
    QByteArray filtered;
    filtered.reserve(payload.size());
    for (char ch : payload) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte >= 32 && byte <= 126) {
            filtered.append(ch);
        }
    }
    return QString::fromLatin1(filtered).trimmed();
}

QString DataQueryPage::tryDecodeFloatValue(const QByteArray &payload) const
{
    if (payload.size() < 4) {
        return {};
    }

    float value = 0.0f;
    quint8 rawBytes[4] = {
        static_cast<quint8>(payload.at(0)),
        static_cast<quint8>(payload.at(1)),
        static_cast<quint8>(payload.at(2)),
        static_cast<quint8>(payload.at(3))
    };
    SwapDateByte(rawBytes, 4);
    memcpy(&value, rawBytes, sizeof(value));

    if (!qIsFinite(value)) {
        return {};
    }

    return QString::number(value, 'f', 3);
}

bool DataQueryPage::decodeImagePacketFrame(uint8_t afn, ImagePacketInfo &info) const
{
    if (afn != 0x36) {
        return false;
    }

    const QByteArray frame = currentFrameBytes();
    if (frame.size() < 26) {
        return false;
    }

    const quint8 *bytes = reinterpret_cast<const quint8 *>(frame.constData());
    const int dataLen =
        (static_cast<int>(bytes[11]) << 8) |
        static_cast<int>(bytes[12]);
    const int packetTotal = decodePackedPacketTotal(bytes[14], bytes[15]);
    const int packetPos = decodePackedPacketPos(bytes[15], bytes[16]);
    if (dataLen <= 0 || packetTotal <= 0 || packetPos <= 0 || packetPos > packetTotal) {
        return false;
    }

    const bool bodyOnlyPacket =
        packetPos != 1 &&
        frame.size() > 26 &&
        !(bytes[25] == 0xF1 && bytes[26] == 0xF1);
    const int payloadOffset = bodyOnlyPacket ? 17 : 25;
    const int payloadLen = dataLen - (bodyOnlyPacket ? 3 : 11);
    if (payloadLen <= 0 || payloadOffset + payloadLen > frame.size()) {
        return false;
    }

    QByteArray packetData = frame.mid(payloadOffset, payloadLen);
    const int markerPos = packetData.indexOf(QByteArray::fromHex("F3F3"));
    if (markerPos >= 0) {
        packetData = packetData.mid(markerPos + 2);
    }

    info.valid = true;
    info.packetTotal = packetTotal;
    info.packetPos = packetPos;
    info.bodyOnlyPacket = bodyOnlyPacket;
    info.packetData = packetData;
    return true;
}

void DataQueryPage::resetImageAssembly()
{
    m_imagePacketTotal = 0;
    m_imagePacketMap.clear();
}

QByteArray DataQueryPage::appendImagePacket(const ImagePacketInfo &info, bool &completed, bool &accepted)
{
    completed = false;
    accepted = false;
    if (!info.valid || info.packetTotal <= 0 || info.packetPos <= 0 || info.packetData.isEmpty()) {
        return {};
    }

    if (info.packetPos == 1 || (m_imagePacketTotal > 0 && m_imagePacketTotal != info.packetTotal)) {
        resetImageAssembly();
    }
    if (m_imagePacketTotal <= 0) {
        m_imagePacketTotal = info.packetTotal;
    }

    if (m_imagePacketMap.contains(info.packetPos)) {
        return {};
    }

    m_imagePacketMap.insert(info.packetPos, info.packetData);
    accepted = true;

    if (m_imagePacketMap.size() != m_imagePacketTotal) {
        return {};
    }

    QByteArray imageData;
    for (int packetIndex = 1; packetIndex <= m_imagePacketTotal; ++packetIndex) {
        if (!m_imagePacketMap.contains(packetIndex)) {
            return {};
        }
        imageData.append(m_imagePacketMap.value(packetIndex));
    }

    completed = true;
    return normalizeJpegPayload(imageData);
}

void DataQueryPage::handleRealtimeDataResponse(uint8_t afn)
{
    if (afn == 0x36) {
        m_waitingImageResponse = false;
        m_imageFallbackAttempted = false;
        if (m_imageRetryTimer != nullptr) {
            m_imageRetryTimer->stop();
        }
    }

    const SensorRecord *record = currentSensor();
    const QByteArray payload = currentPayload();
    ImagePacketInfo imagePacket;
    if (decodeImagePacketFrame(afn, imagePacket)) {
        bool completed = false;
        bool accepted = false;
        const QByteArray imageData = appendImagePacket(imagePacket, completed, accepted);
        const bool packetHasJpegStart = hasJpegStartMarker(imagePacket.packetData);
        const bool imageHasJpegEnd = completed && containsJpegEndMarker(imageData);

        clearResults();
        showResultContent();
        if (record != nullptr) {
            appendResultRow(zh(u8"\u4f20\u611f\u5668\u540d\u79f0"),
                            !record->name.trimmed().isEmpty() ? record->name : record->id);
            appendResultRow(zh(u8"\u91c7\u96c6\u8981\u7d20"),
                            record->element.isEmpty() ? record->name : record->element);
        }
        appendResultRow(zh(u8"\u67e5\u8be2\u65f6\u95f4"), formatMessageTime());
        appendResultRow(zh(u8"\u603b\u5305\u6570"), QString::number(imagePacket.packetTotal));
        appendResultRow(zh(u8"\u5f53\u524d\u5305\u5e8f"), QString::number(imagePacket.packetPos));
        appendResultRow(zh(u8"\u5df2\u6536\u5305\u6570"), QString::number(m_imagePacketMap.size()));
        if (imagePacket.packetPos == 1) {
            appendResultRow(zh(u8"JPEG \u8d77\u59cb"),
                            packetHasJpegStart ? zh(u8"\u5df2\u68c0\u6d4b FF D8") : zh(u8"\u9996\u5305\u672a\u76f4\u63a5\u547d\u4e2d FF D8"));
        }

        if (completed && !imageData.isEmpty()) {
            appendResultRow(zh(u8"JPEG \u7ed3\u675f"),
                            imageHasJpegEnd ? zh(u8"\u5df2\u68c0\u6d4b FF D9") : zh(u8"\u672a\u68c0\u6d4b\u5230 FF D9"));
            appendResultRow(zh(u8"\u72b6\u6001"),
                            imageHasJpegEnd
                                ? zh(u8"\u56fe\u7247\u5206\u5305\u5df2\u6536\u9f50")
                                : zh(u8"\u56fe\u7247\u5206\u5305\u5df2\u6536\u9f50\uff0c\u4f46 JPEG \u7ed3\u5c3e\u4e0d\u5b8c\u6574"));
            updateImagePreview(imageData);
            setStatusText(imageHasJpegEnd
                              ? zh(u8"\u5df2\u6536\u9f50 %1/%1 \u4e2a\u56fe\u7247\u5206\u5305\uff0c\u6b63\u5728\u663e\u793a")
                                    .arg(m_imagePacketTotal)
                              : zh(u8"\u5df2\u6536\u9f50 %1/%1 \u4e2a\u56fe\u7247\u5206\u5305\uff0c\u4f46 JPEG \u7ed3\u5c3e\u4e0d\u5b8c\u6574")
                                    .arg(m_imagePacketTotal),
                          !imageHasJpegEnd);
            return;
        }

        appendResultRow(zh(u8"\u72b6\u6001"),
                        accepted
                            ? zh(u8"\u5df2\u63a5\u6536\u56fe\u7247\u5206\u5305\uff0c\u7b49\u5f85\u540e\u7eed\u6570\u636e")
                            : zh(u8"\u5206\u5305\u91cd\u590d\u6216\u672a\u91c7\u7eb3"));
        setStatusText(zh(u8"\u5df2\u63a5\u6536\u56fe\u7247\u5206\u5305 %1/%2\uff0c\u5f53\u524d\u7d2f\u8ba1 %3/%2")
                          .arg(imagePacket.packetPos)
                          .arg(imagePacket.packetTotal)
                          .arg(m_imagePacketMap.size()),
                      !accepted);
        return;
    }

    const DecodedRealtimeData decoded = decodeRealtimePayload(payload, record);

    clearResults();
    showResultContent();
    if (record != nullptr) {
        appendResultRow(zh(u8"\u4f20\u611f\u5668\u540d\u79f0"),
                        !record->name.trimmed().isEmpty() ? record->name : record->id);
        appendResultRow(zh(u8"\u91c7\u96c6\u8981\u7d20"),
                        record->element.isEmpty() ? record->name : record->element);
        if (!record->port.isEmpty()) {
            appendResultRow(zh(u8"\u63a5\u5165\u7aef\u53e3"), record->port);
        }
    }

    appendResultRow(zh(u8"\u67e5\u8be2\u65f6\u95f4"), formatMessageTime());
    if (!decoded.sampleTimeText.isEmpty()) {
        appendResultRow(zh(u8"\u91c7\u96c6\u65f6\u95f4"), decoded.sampleTimeText);
    }

    if (payload.isEmpty()) {
        appendResultRow(zh(u8"\u72b6\u6001"), zh(u8"\u8fd4\u56de\u7a7a\u8f7d\u8377"));
        setStatusText(zh(u8"\u8bbe\u5907\u8fd4\u56de\u4e86\u7a7a\u6570\u636e"), true);
        return;
    }

    if (decoded.isImage) {
        appendResultRow(zh(u8"\u72b6\u6001"), decoded.statusText);
        updateImagePreview(decoded.imagePayload);
        setStatusText(decoded.ok
                          ? zh(u8"\u5df2\u63a5\u6536\u56fe\u7247\u6570\u636e")
                          : zh(u8"\u56fe\u7247\u6570\u636e\u89e3\u6790\u5931\u8d25"),
                      !decoded.ok);
        return;
    }

    if (decoded.ok) {
        appendResultRow(zh(u8"\u5f53\u524d\u503c"), decoded.valueText);
        if (!decoded.unitText.isEmpty()) {
            appendResultRow(zh(u8"\u5355\u4f4d"), decoded.unitText);
        }
        appendResultRow(zh(u8"\u72b6\u6001"), decoded.statusText);
        setStatusText(zh(u8"\u5df2\u63a5\u6536 %1 \u7684\u5b9e\u65f6\u6570\u636e")
                          .arg(record != nullptr && !record->name.isEmpty() ? record->name : zh(u8"\u4f20\u611f\u5668")));
        return;
    }

    const QString printableText = extractPrintableText(payload);
    const QString floatValue = tryDecodeFloatValue(payload);
    appendResultRow(zh(u8"\u72b6\u6001"), decoded.statusText.isEmpty() ? zh(u8"\u89e3\u6790\u5931\u8d25") : decoded.statusText);
    if (!printableText.isEmpty()) {
        appendResultRow(zh(u8"\u8fd4\u56de\u6587\u672c"), printableText);
    } else if (!floatValue.isEmpty()) {
        appendResultRow(zh(u8"\u5019\u9009\u6570\u503c"), floatValue);
    }
    setStatusText(zh(u8"\u8bbe\u5907\u5df2\u8fd4\u56de\uff0c\u4f46\u672c\u9875\u6682\u672a\u5b8c\u5168\u89e3\u6790 AFN 0x%1")
                      .arg(afn, 2, 16, QLatin1Char('0')).toUpper(),
                  true);
}
