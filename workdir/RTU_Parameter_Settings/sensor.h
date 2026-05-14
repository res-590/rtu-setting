#ifndef SENSOR_H
#define SENSOR_H

#include <QMap>
#include <QVector>
#include <QWidget>

class QComboBox;
class QFrame;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

namespace Ui {
class sensor;
}

#pragma pack(push)
#pragma pack(1)
struct SensorConfigFrame
{
    qint8 sensorCom;
    qint16 sensorType;
    quint8 sensor485Addr;
    quint8 sensorInstance[16];
    float sensorInstallAngle;
    float sensorBaseValue;
    float sensorResolution;
    quint8 sensorAppendValue;
    float sensorLowerThreshold;
    float sensorUpperThreshold;
};
#pragma pack(pop)

struct SensorRecord
{
    QString id;
    QString name;
    QString port;
    QString model;
    QString element;
    QString address485;
    QString angle;
    QString baseValue;
    QString resolution;
    QString upperAlarm;
    QString lowerAlarm;
    QString extraData;
    int portCode = -1;
    int modelCode = -1;
    int elementCode = -1;
    QVector<int> elementCodes;
    int appendCode = -1;
};

class sensor : public QWidget
{
    Q_OBJECT

public:
    explicit sensor(QWidget *parent = nullptr);
    ~sensor();

    void handleSensorInfo();
    QVector<SensorRecord> sensorRecords() const;

signals:
    void sensorRecordsChanged();

private slots:
    void on_add_sensor_clicked();
    void on_set_Button_clicked();
    void on_delete_sensor_clicked();

private:
    void buildListPage();
    void refreshSensorTable();
    void showSensorDialog(int editRow = -1);
    void requestSensorParamRead();
    void sendSensorConfigWrite();
    void loadPortMappings();
    void loadModelMappings();
    void loadElementMappings();
    void loadAppendMappings();
    QString portTextFromCode(int code) const;
    QString modelTextFromCode(int code) const;
    QString elementTextFromCode(int code) const;
    QString appendTextFromCode(int code) const;
    QVector<int> elementCodesForModel(int modelCode) const;
    int elementCodeFromText(const QString &text) const;
    int portCodeFromText(const QString &text) const;
    int modelCodeFromText(const QString &text) const;
    int appendCodeFromText(const QString &text) const;
    QString makeSensorIdentifier(int portCode, int modelCode, int address485) const;
    SensorConfigFrame frameFromRecord(const SensorRecord &record) const;
    SensorRecord recordFromFrame(const SensorConfigFrame &frame, int rowIndex) const;
    QStringList comboItems(QComboBox *combo) const;
    QString nextSensorId();

    Ui::sensor *ui;
    QVBoxLayout *m_pageLayout = nullptr;
    QFrame *m_toolbarCard = nullptr;
    QPushButton *m_readButton = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QTableWidget *m_sensorTable = nullptr;
    QVector<SensorRecord> m_records;
    int m_nextSensorIndex = 1;
    QMap<int, QString> m_portCodeToText;
    QMap<QString, int> m_portTextToCode;
    QMap<int, QString> m_modelCodeToText;
    QMap<QString, int> m_modelTextToCode;
    QMap<int, QString> m_elementCodeToText;
    QMap<QString, int> m_elementTextToCode;
    QMap<int, QVector<int>> m_modelCodeToElementCodes;
    QMap<int, QString> m_appendCodeToText;
    QMap<QString, int> m_appendTextToCode;
};

#endif // SENSOR_H
