#ifndef PORTSET_H
#define PORTSET_H

#include <QWidget>

namespace Ui {
class portset;
}

#pragma pack(push)
#pragma pack(1) // 单字节对齐
typedef struct {
    uint8_t Port1_Type;
    uint32_t Port1_Buand;
    uint8_t Port1_RF;
    uint8_t Port1_Reserve;
    uint8_t Port2_Type;
    uint32_t Port2_Buand;
    uint8_t Port2_RF;
    uint8_t Port2_Reserve;
    uint8_t Port3_Type;
    uint32_t Port3_Buand;
    uint8_t Port3_RF;
    uint8_t Port3_Reserve;
    uint8_t Port4_Type;
    uint32_t Port4_Buand;
    uint8_t Port4_RF;
    uint8_t Port4_Reserve;
    uint8_t Port5_Type;
    uint32_t Port5_Buand;
    uint8_t Port5_RF;
    uint8_t Port5_Reserve;
} s_PortsetInfo;

typedef union {
    s_PortsetInfo portinfo;
    uint8_t date[35];
} Port_Protocol;
#pragma pack(pop)

class portset : public QWidget
{
    Q_OBJECT

public:
    explicit portset(QWidget *parent = nullptr);
    ~portset();
    void handle_RTUINFO(void);

private slots:
    void on_set_Button_clicked();
    void on_equit_type1_currentIndexChanged(int index);
    void on_equit_type2_currentIndexChanged(int index);
    void on_equit_type3_currentIndexChanged(int index);
    void on_equit_type4_currentIndexChanged(int index);
    void on_equit_type5_currentIndexChanged(int index);
    void on_baud_rate1_currentIndexChanged(int index);
    void on_baud_rate2_currentIndexChanged(int index);
    void on_baud_rate3_currentIndexChanged(int index);
    void on_baud_rate4_currentIndexChanged(int index);
    void on_baud_rate5_currentIndexChanged(int index);
    void on_RF1_currentIndexChanged(int index);
    void on_RF2_currentIndexChanged(int index);
    void on_RF3_currentIndexChanged(int index);
    void on_RF4_currentIndexChanged(int index);
    void on_RF5_currentIndexChanged(int index);
    void on_keep1_editingFinished();
    void on_keep2_editingFinished();
    void on_keep3_editingFinished();
    void on_keep4_editingFinished();
    void on_keep5_editingFinished();
    void on_read_Button_clicked();

private:
    void syncUiToProtocol();
    uint8_t encodePortTypeIndex(int index) const;
    int decodePortTypeValue(uint8_t value) const;
    void fill_portbuand(Port_Protocol &dtuinfobuffer, int comboxindex);
    Port_Protocol port_info;
    Ui::portset *ui;
};

#endif // PORTSET_H
