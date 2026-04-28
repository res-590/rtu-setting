#ifndef YUNXINGCANSHU_H
#define YUNXINGCANSHU_H

#include <cstdint>
#include <QMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QTimer;

namespace Ui {
class yunxingcanshu;
}

class yunxingcanshu : public QWidget
{
    Q_OBJECT

public:
    explicit yunxingcanshu(QWidget *parent = nullptr);
    ~yunxingcanshu();

    void handleQueryTimeResponse();
    void handleAdjustTimeResponse();
    void handleVersionResponse();
    void handleStatusResponse();
    void handleFormatSdResponse();
    void handleRestoreFactoryResponse();
    void handleRestartResponse();
    void handleOutPortControlResponse();
    void handleOutPortQueryResponse();
    void handleUpgradeResponse();

private slots:
    void on_set_Button_clicked();

private:
    enum OperationId {
        QueryTimeOperation = 0,
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

    struct OperationRow {
        QLabel *resultLabel = nullptr;
        QPushButton *actionButton = nullptr;
        QComboBox *comboA = nullptr;
        QComboBox *comboB = nullptr;
        QLineEdit *lineEdit = nullptr;
        QLabel *prefixLabel = nullptr;
        QLabel *suffixLabel = nullptr;
        QPushButton *auxButton = nullptr;
        QPushButton *cancelButton = nullptr;
    };

    void setupOperationTable();
    QWidget *createOperationButton(OperationId id);
    QWidget *createResultWidget(OperationId id);
    QWidget *createPlainResultWidget(OperationId id);
    QWidget *createInputPortConfigWidget(OperationId id);
    QWidget *createOutPortControlWidget(OperationId id);
    QWidget *createUpgradeWidget(OperationId id);
    void setResult(OperationId id, const QString &text);
    QByteArray currentPayload() const;
    QString formatMessageTime() const;
    QString extractPrintableText(const QByteArray &payload) const;
    void sendFrame(uint8_t afn, const QByteArray &payload = QByteArray(), int8_t sFlag = 0x02);
    bool encodeUpgradeFrame(uint8_t *frame,
                            uint32_t &length,
                            int packetIndex,
                            const QByteArray &packetData = QByteArray());
    void sendUpgradePacket(int packetIndex);
    void beginUpgradeTransfer();
    void cancelUpgradeTransfer();
    void resetUpgradeState(bool clearCancelled = true);
    void triggerOperation(OperationId id);
    void chooseUpgradeFile();

    Ui::yunxingcanshu *ui;
    QMap<int, OperationRow> m_rows;
    QString m_upgradeFilePath;
    QByteArray m_upgradeFileData;
    int m_upgradeTotalPackets = 0;
    int m_upgradeCurrentPacket = 0;
    uint16_t m_upgradeSerial = 0;
    bool m_upgradeInProgress = false;
    bool m_upgradeCancelled = false;
};

#endif // YUNXINGCANSHU_H
