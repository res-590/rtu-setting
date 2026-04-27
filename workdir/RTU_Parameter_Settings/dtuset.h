#ifndef DTUSET_H
#define DTUSET_H

#include <QWidget>

namespace Ui {
class dtuset;
}

#pragma pack(push)
#pragma pack(1) // 单字节对齐
typedef struct {
    uint8_t run_model;              ///< 运行模式
    uint32_t baundrate;             ///< DTU 波特率
    uint16_t Max_Datepacket;        ///< 最大数据包
    uint8_t server_number[12];      ///< 短信中心号码
    uint8_t message_CompileType;    ///< 短信编码方式
    uint8_t ch1_in_type;            ///< 通道 1 通信方式
    uint32_t ch1_in_ipaddr;         ///< 通道 1 IP 地址
    uint8_t ch1_in_dname[65];       ///< 通道 1 域名
    uint16_t ch1_in_port;           ///< 通道 1 端口
    uint8_t ch1_number[12];         ///< 通道 1 服务号码
    uint8_t ch2_in_type;            ///< 通道 2 通信方式
    uint32_t ch2_in_ipaddr;         ///< 通道 2 IP 地址
    uint8_t ch2_in_dname[65];       ///< 通道 2 域名
    uint16_t ch2_in_port;           ///< 通道 2 端口
    uint8_t ch2_number[12];         ///< 通道 2 服务号码
    uint8_t ch3_in_type;            ///< 通道 3 通信方式
    uint32_t ch3_in_ipaddr;         ///< 通道 3 IP 地址
    uint8_t ch3_in_dname[65];       ///< 通道 3 域名
    uint16_t ch3_in_port;           ///< 通道 3 端口
    uint8_t ch3_number[12];         ///< 通道 3 服务号码
    uint8_t ch4_in_type;            ///< 通道 4 通信方式
    uint32_t ch4_in_ipaddr;         ///< 通道 4 IP 地址
    uint8_t ch4_in_dname[65];       ///< 通道 4 域名
    uint16_t ch4_in_port;           ///< 通道 4 端口
    uint8_t ch4_number[12];         ///< 通道 4 服务号码
} DTU_ConfigInfo_s;

typedef union {
    DTU_ConfigInfo_s DTU_info;
    uint8_t date[356];
} RTU_Protocol;
#pragma pack(pop)

class dtuset : public QWidget
{
    Q_OBJECT

public:
    dtuset(QWidget *parent = nullptr);
    ~dtuset();
    void handle_DTUINFO(void);

private slots:
    void on_set_Button_clicked();
    void on_channel1_currentIndexChanged(int index);
    void on_channel2_currentIndexChanged(int index);
    void on_channel3_currentIndexChanged(int index);
    void on_channel4_currentIndexChanged(int index);
    void on_IP1_editingFinished();
    void on_IP2_editingFinished();
    void on_IP3_editingFinished();
    void on_IP4_editingFinished();
    void on_domain3_editingFinished();
    void on_domain1_editingFinished();
    void on_domain2_editingFinished();
    void on_domain4_editingFinished();
    void on_sms_num1_editingFinished();
    void on_port1_editingFinished();
    void on_port2_editingFinished();
    void on_port3_editingFinished();
    void on_port4_editingFinished();
    void on_sms_num2_editingFinished();
    void on_sms_num3_editingFinished();
    void on_sms_num4_editingFinished();
    void on_run_model_currentIndexChanged(int index);
    void on_baud_rate_currentIndexChanged(int index);
    void on_Max_data_editingFinished();
    void on_SMS_editingFinished();
    void on_sms_code_currentIndexChanged(int index);
    void on_read_Button_clicked();

private:
    void syncUiToProtocol();
    RTU_Protocol dtuinfo;
    Ui::dtuset *ui;
};

#endif // DTUSET_H
