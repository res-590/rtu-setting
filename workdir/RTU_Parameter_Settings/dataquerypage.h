#ifndef DATAQUERYPAGE_H
#define DATAQUERYPAGE_H

#include "sensor.h"

#include <cstdint>
#include <QByteArray>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTableWidget;

class DataQueryPage : public QWidget
{
    Q_OBJECT

public:
    explicit DataQueryPage(QWidget *parent = nullptr);

    void refreshSensorOptions();
    void handleRealtimeDataResponse(uint8_t afn);

private:
    struct SensorOption {
        QString displayText;
        SensorRecord record;
    };

    void buildUi();
    void appendResultRow(const QString &field, const QString &value);
    void setStatusText(const QString &text, bool isError = false);
    void renderSensorInfo(const SensorRecord &record);
    void requestRealtimeData();
    QByteArray currentPayload() const;
    QString formatMessageTime() const;
    QString extractPrintableText(const QByteArray &payload) const;
    QString tryDecodeFloatValue(const QByteArray &payload) const;
    const SensorRecord *currentSensor() const;

    QButtonGroup *m_queryModeGroup = nullptr;
    QRadioButton *m_realtimeRadio = nullptr;
    QComboBox *m_sensorCombo = nullptr;
    QPushButton *m_queryButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QVector<SensorOption> m_sensorOptions;
};

#endif // DATAQUERYPAGE_H
