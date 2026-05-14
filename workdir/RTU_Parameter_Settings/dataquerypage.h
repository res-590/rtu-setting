#ifndef DATAQUERYPAGE_H
#define DATAQUERYPAGE_H

#include "sensor.h"

#include <QByteArray>
#include <QWidget>
#include <cstdint>

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTableWidget;
class QTimer;

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

    struct ImagePacketInfo {
        bool valid = false;
        int packetTotal = 0;
        int packetPos = 0;
        bool bodyOnlyPacket = false;
        QByteArray packetData;
    };

    void buildUi();
    void appendResultRow(const QString &field, const QString &value);
    void clearResults();
    void setStatusText(const QString &text, bool isError = false);
    void showEmptyState(const QString &title, const QString &hint = QString());
    void showResultContent();
    void updateImagePreview(const QByteArray &payload);
    void renderSensorInfo(const SensorRecord &record);
    void requestSensorParamRead();
    void requestRealtimeData();
    void sendSpecifiedRealtimeQuery(const SensorRecord &record);
    void sendImageQueryFrame(const SensorRecord &record, bool withCameraExtension);
    QByteArray currentPayload() const;
    QByteArray currentFrameBytes() const;
    QString formatMessageTime() const;
    QString extractPrintableText(const QByteArray &payload) const;
    QString tryDecodeFloatValue(const QByteArray &payload) const;
    const SensorRecord *currentSensor() const;
    bool decodeImagePacketFrame(uint8_t afn, ImagePacketInfo &info) const;
    void resetImageAssembly();
    QByteArray appendImagePacket(const ImagePacketInfo &info, bool &completed, bool &accepted);

    QComboBox *m_sensorCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_queryButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QStackedWidget *m_resultStack = nullptr;
    QWidget *m_emptyStatePage = nullptr;
    QLabel *m_emptyTitleLabel = nullptr;
    QLabel *m_emptyHintLabel = nullptr;
    QWidget *m_resultContentPage = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QFrame *m_imageCard = nullptr;
    QScrollArea *m_imageScrollArea = nullptr;
    QLabel *m_imagePreviewLabel = nullptr;
    QLabel *m_imageMetaLabel = nullptr;
    QTimer *m_imageRetryTimer = nullptr;

    QVector<SensorOption> m_sensorOptions;
    int m_imagePacketTotal = 0;
    QMap<int, QByteArray> m_imagePacketMap;
    bool m_waitingImageResponse = false;
    bool m_imageFallbackAttempted = false;
    SensorRecord m_pendingImageRecord;
};

#endif // DATAQUERYPAGE_H
