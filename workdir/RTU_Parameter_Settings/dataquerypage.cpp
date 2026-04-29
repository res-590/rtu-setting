#include "dataquerypage.h"

#include "RTU_ParameterSetting.h"
#include "yunxingcanshu.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtGlobal>

#include <cstring>

namespace {
QString zh(const char *text)
{
    return QString::fromUtf8(text);
}

QWidget *createSectionHeader(const QString &title, QWidget *parent = nullptr)
{
    auto *container = new QFrame(parent);
    container->setStyleSheet(QStringLiteral(
        "QFrame{background:#f7f9fc;border:none;border-radius:4px;}"
        "QLabel{font-size:15px;font-weight:700;color:#1f2937;}"));

    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    auto *accent = new QFrame(container);
    accent->setFixedSize(4, 18);
    accent->setStyleSheet(QStringLiteral("background:#2f78e8;border-radius:2px;"));

    auto *label = new QLabel(title, container);
    layout->addWidget(accent, 0, Qt::AlignVCenter);
    layout->addWidget(label, 0, Qt::AlignVCenter);
    layout->addStretch();
    return container;
}
}

DataQueryPage::DataQueryPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    refreshSensorOptions();
}

void DataQueryPage::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(14);

    auto *queryCard = new QFrame(this);
    queryCard->setFrameShape(QFrame::StyledPanel);
    queryCard->setStyleSheet(QStringLiteral(
        "QFrame{background:#ffffff;border:1px solid #e8eef6;border-radius:6px;}"
        "QGroupBox{border:none;font-size:14px;color:#111827;font-weight:500;margin-top:0px;}"
        "QLabel{color:#111827;font-size:14px;}"
        "QRadioButton{color:#2f78e8;font-size:14px;font-weight:600;spacing:8px;}"
        "QRadioButton::indicator{width:14px;height:14px;}"
        "QRadioButton::indicator:unchecked{border:2px solid #98a2b3;border-radius:7px;background:#ffffff;}"
        "QRadioButton::indicator:checked{border:4px solid #2f78e8;border-radius:7px;background:#ffffff;}"
        "QComboBox{min-height:38px;border:1px solid #d0d5dd;border-radius:4px;padding:0 12px;background:#ffffff;color:#344054;}"
        "QComboBox::drop-down{border:none;width:28px;}"
        "QPushButton{min-width:78px;min-height:32px;background:#2f78e8;color:#ffffff;border:none;border-radius:4px;padding:4px 16px;font-size:14px;font-weight:600;}"
        "QPushButton:hover{background:#2468cc;}"));
    auto *queryCardLayout = new QVBoxLayout(queryCard);
    queryCardLayout->setContentsMargins(0, 0, 0, 0);
    queryCardLayout->setSpacing(0);
    queryCardLayout->addWidget(createSectionHeader(zh(u8"\u6570\u636e\u67e5\u8be2"), queryCard));

    auto *queryBox = new QGroupBox(zh(u8"\u6570\u636e\u67e5\u8be2"), queryCard);
    auto *queryLayout = new QGridLayout(queryBox);
    queryLayout->setHorizontalSpacing(18);
    queryLayout->setVerticalSpacing(24);
    queryLayout->setContentsMargins(36, 26, 36, 26);
    queryLayout->setColumnMinimumWidth(0, 72);
    queryLayout->setColumnMinimumWidth(1, 140);

    auto *modeLabel = new QLabel(zh(u8"\u6570\u636e\u67e5\u8be2"), queryBox);
    m_realtimeRadio = new QRadioButton(zh(u8"\u5b9e\u65f6\u6570\u636e"), queryBox);
    m_realtimeRadio->setChecked(true);
    m_queryModeGroup = new QButtonGroup(this);
    m_queryModeGroup->addButton(m_realtimeRadio);

    auto *sensorLabel = new QLabel(zh(u8"\u4f20\u611f\u5668"), queryBox);
    m_sensorCombo = new QComboBox(queryBox);
    m_sensorCombo->setMinimumWidth(420);
    m_sensorCombo->setMaximumWidth(520);

    m_queryButton = new QPushButton(zh(u8"\u67e5\u8be2"), queryBox);
    connect(m_queryButton, &QPushButton::clicked, this, &DataQueryPage::requestRealtimeData);

    queryLayout->addWidget(modeLabel, 0, 0);
    queryLayout->addWidget(m_realtimeRadio, 0, 1, 1, 2, Qt::AlignLeft);
    queryLayout->addWidget(sensorLabel, 1, 0);
    queryLayout->addWidget(m_sensorCombo, 1, 1, 1, 2);
    queryLayout->addWidget(m_queryButton, 2, 1, 1, 1, Qt::AlignLeft);

    queryCardLayout->addWidget(queryBox);
    rootLayout->addWidget(queryCard);

    auto *resultCard = new QFrame(this);
    resultCard->setFrameShape(QFrame::StyledPanel);
    resultCard->setStyleSheet(QStringLiteral(
        "QFrame{background:#ffffff;border:1px solid #e8eef6;border-radius:6px;}"
        "QLabel{color:#111827;}"
        "QTableWidget{border:none;background:#ffffff;gridline-color:#edf2f7;color:#344054;}"
        "QHeaderView::section{background:#f8fafc;color:#475467;border:none;border-bottom:1px solid #eaecf0;padding:10px 12px;font-weight:700;}"
        "QTableWidget::item{padding:10px 12px;border-bottom:1px solid #f2f4f7;}"));
    auto *resultCardLayout = new QVBoxLayout(resultCard);
    resultCardLayout->setContentsMargins(0, 0, 0, 0);
    resultCardLayout->setSpacing(0);
    resultCardLayout->addWidget(createSectionHeader(zh(u8"\u67e5\u8be2\u7ed3\u679c"), resultCard));

    m_statusLabel = new QLabel(zh(u8"\u8bf7\u9009\u62e9\u4f20\u611f\u5668\u540e\u6267\u884c\u67e5\u8be2\u3002"), resultCard);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#667085;font-size:13px;padding:18px 24px 8px 24px;"));
    resultCardLayout->addWidget(m_statusLabel);

    m_resultTable = new QTableWidget(resultCard);
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
    m_resultTable->setMinimumHeight(320);
    m_resultTable->setFrameShape(QFrame::NoFrame);
    m_resultTable->setContentsMargins(18, 0, 18, 18);
    resultCardLayout->addWidget(m_resultTable);

    rootLayout->addWidget(resultCard);
    rootLayout->addStretch();
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

void DataQueryPage::setStatusText(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError
                                     ? QStringLiteral("color:#b42318;")
                                     : QStringLiteral("color:#475569;"));
}

void DataQueryPage::refreshSensorOptions()
{
    const QString previousId = currentSensor() != nullptr ? currentSensor()->id : QString();
    m_sensorOptions.clear();
    m_sensorCombo->clear();

    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_sensorPage == nullptr) {
        setStatusText(zh(u8"\u4f20\u611f\u5668\u9875\u672a\u521d\u59cb\u5316\uff0c\u6682\u65f6\u65e0\u6cd5\u52a0\u8f7d\u67e5\u8be2\u5bf9\u8c61\u3002"), true);
        return;
    }

    const QVector<SensorRecord> records = m_RTU_ParameterSetting->m_sensorPage->sensorRecords();
    for (const SensorRecord &record : records) {
        SensorOption option;
        option.record = record;
        option.displayText = QStringLiteral("%1 - %2(%3)")
                                 .arg(record.element.isEmpty() ? record.name : record.element,
                                      record.model,
                                      record.id);
        m_sensorOptions.push_back(option);
        m_sensorCombo->addItem(option.displayText);
    }

    for (int i = 0; i < m_sensorOptions.size(); ++i) {
        if (m_sensorOptions.at(i).record.id == previousId) {
            m_sensorCombo->setCurrentIndex(i);
            break;
        }
    }

    if (m_sensorOptions.isEmpty()) {
        setStatusText(zh(u8"\u6682\u65e0\u4f20\u611f\u5668\u914d\u7f6e\uff0c\u8bf7\u5148\u5728\u201c\u4f20\u611f\u5668\u53c2\u6570\u201d\u9875\u9762\u8bfb\u53d6\u6216\u914d\u7f6e\u4f20\u611f\u5668\u3002"), true);
    } else {
        setStatusText(zh(u8"\u5df2\u52a0\u8f7d %1 \u4e2a\u4f20\u611f\u5668\uff0c\u53ef\u6267\u884c\u5b9e\u65f6\u6570\u636e\u67e5\u8be2\u3002").arg(m_sensorOptions.size()));
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
    appendResultRow(zh(u8"\u4f20\u611f\u5668\u6807\u8bc6"), record.id);
    appendResultRow(zh(u8"\u91c7\u96c6\u8981\u7d20"), record.element.isEmpty() ? record.name : record.element);
    appendResultRow(zh(u8"\u4f20\u611f\u5668\u578b\u53f7"), record.model);
    appendResultRow(zh(u8"\u63a5\u5165\u7aef\u53e3"), record.port);
    appendResultRow(zh(u8"485 \u5730\u5740"), record.address485);
}

void DataQueryPage::requestRealtimeData()
{
    refreshSensorOptions();

    const SensorRecord *record = currentSensor();
    if (record == nullptr) {
        m_resultTable->setRowCount(0);
        setStatusText(zh(u8"\u6ca1\u6709\u53ef\u67e5\u8be2\u7684\u4f20\u611f\u5668\u3002"), true);
        return;
    }

    m_resultTable->setRowCount(0);
    renderSensorInfo(*record);
    appendResultRow(zh(u8"\u67e5\u8be2\u6a21\u5f0f"), zh(u8"\u5b9e\u65f6\u6570\u636e"));
    appendResultRow(zh(u8"\u72b6\u6001"), zh(u8"\u7b49\u5f85\u8bbe\u5907\u8fd4\u56de\u5b9e\u65f6\u6570\u636e\u62a5\u6587"));
    setStatusText(zh(u8"\u5df2\u53d1\u9001\u5b9e\u65f6\u6570\u636e\u67e5\u8be2\uff0c\u7b49\u5f85\u8bbe\u5907\u8fd4\u56de\u3002"));

    if (m_RTU_ParameterSetting == nullptr || m_RTU_ParameterSetting->m_runtimePage == nullptr) {
        setStatusText(zh(u8"\u8fd0\u884c\u53c2\u6570\u9875\u672a\u521d\u59cb\u5316\uff0c\u65e0\u6cd5\u53d1\u9001\u67e5\u8be2\u547d\u4ee4\u3002"), true);
        return;
    }

    m_RTU_ParameterSetting->m_runtimePage->sendFrame(AFN_QUERYREALDATA);
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

void DataQueryPage::handleRealtimeDataResponse(uint8_t afn)
{
    const SensorRecord *record = currentSensor();
    const QByteArray payload = currentPayload();

    m_resultTable->setRowCount(0);
    if (record != nullptr) {
        renderSensorInfo(*record);
    }

    appendResultRow(zh(u8"\u67e5\u8be2\u65f6\u95f4"), formatMessageTime());
    appendResultRow(zh(u8"\u8fd4\u56de AFN"), QStringLiteral("0x%1").arg(afn, 2, 16, QLatin1Char('0')).toUpper());

    if (payload.isEmpty()) {
        appendResultRow(zh(u8"\u72b6\u6001"), zh(u8"\u8bbe\u5907\u5df2\u8fd4\u56de\uff0c\u4f46\u62a5\u6587\u8f7d\u8377\u4e3a\u7a7a"));
        setStatusText(zh(u8"\u8bbe\u5907\u8fd4\u56de\u4e86\u7a7a\u6570\u636e\u62a5\u6587\u3002"), true);
        return;
    }

    appendResultRow(zh(u8"\u539f\u59cb HEX"), QString::fromLatin1(payload.toHex(' ').toUpper()));

    const QString printableText = extractPrintableText(payload);
    if (!printableText.isEmpty()) {
        appendResultRow(zh(u8"\u6587\u672c\u6570\u636e"), printableText);
    }

    const QString floatValue = tryDecodeFloatValue(payload);
    if (!floatValue.isEmpty()) {
        appendResultRow(zh(u8"\u5019\u9009\u6570\u503c"), floatValue);
    }

    appendResultRow(zh(u8"\u8f7d\u8377\u957f\u5ea6"), zh(u8"%1 \u5b57\u8282").arg(payload.size()));
    appendResultRow(zh(u8"\u72b6\u6001"), zh(u8"\u5b9e\u65f6\u6570\u636e\u5df2\u8fd4\u56de"));
    setStatusText(zh(u8"\u5df2\u63a5\u6536\u5230\u5b9e\u65f6\u6570\u636e\u8fd4\u56de\u3002"));
}
