#ifndef BASICPAGE_H
#define BASICPAGE_H

#include <QWidget>
#include <QByteArray>
#include <QString>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui { class BasicPage; }
QT_END_NAMESPACE

class QCheckBox;

typedef unsigned char BYTE;
typedef struct{
    uint8_t centeraddr;
    uint8_t testaddr[5];
    uint16_t cmcpassword;
    uint8_t worktype;
} s_FrameBaseInfo;

class BasicPage : public QWidget
{
    Q_OBJECT

public:
    explicit BasicPage(QWidget *parent = nullptr);
    ~BasicPage();

    static QByteArray CJ_data;
    static QString str;
    static QByteArray TPzb;
    void requestBaseParamRead();
    void requestRuntimeParamRead();
    void handle_baseConfigParam();
    void handle_runtimeConfigParam();
    void setBaseFrameInfo(const s_FrameBaseInfo &date);

private slots:
    void on_huifu_Button_clicked();
    void on_read_Button_clicked();
    void on_set_Button_clicked();
    void on_selectAllHoursButton_clicked();
    void on_clearAllHoursButton_clicked();
    void on_every2HoursButton_clicked();
    void on_every3HoursButton_clicked();

private:
    QString decodePackedDecimal(const std::vector<uint8_t> &data, uint index, int byteLen, int fracDigits) const;
    void clearFormValues();
    void setHourSelection(int step, int startHour = 0);
    void checkBox_TPzb();
    void setWorkMode(int workType);
    QString testAddrToDeviceId(const uint8_t testaddr[5]) const;
    QList<QCheckBox *> hourCheckBoxes() const;

    Ui::BasicPage *ui;
    bool m_runtimeReadPending = false;
};

#endif // BASICPAGE_H
