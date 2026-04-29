#ifndef RTU_PARAMETERSETTING_H
#define RTU_PARAMETERSETTING_H
#include <QMainWindow>
#include <mainwindow.h>
#include <device_connection.h>
#include <dtuset.h>
#include <QDebug>
#include <QMutex>
 #include <queue>
#include <QTime>
#include "string"
#include "portset.h"
#include "basicpage.h"
class yunxingcanshu;
class sensor;
#define ENQ 0x05
#define STX 0x02
#define AFN_SETRTUPARAM 0xF1
#define AFN_SETDTUPARAM 0xF2
#define AFN_READRTUPARAM 0xF3
#define AFN_READDTUPARAM 0xF4
#define AFN_BASEPARAM 0x40
#define AFN_READBASEPARAM 0x41
#define AFN_SETRUNPARAM 0x42
#define AFN_READRUNRPARAM 0x43
#define AFN_QUERYREALDATA 0x44
#define IDCODE_SENSORCONFIG 0x1F
#define AFN_QUERYVERSION 0x45
#define AFN_QUERYSTATUS 0x46
#define AFN_FORMATSD 0x47
#define AFN_RESETFACTORY 0x48
#define AFN_ADJUSTTIME 0x4A
#define AFN_QUERYTIME 0x51
#define AFN_RESETRTU 0xE0
#define AFN_OUTPORTCONTROL 0xE3
#define AFN_OUTPORTQUERY 0xE4
#define AFN_UPGRADERTU 0xF0
#define MTYPE_UP    0x01
#define MTYPE_DOWN  0x00
#pragma pack(push)
#pragma pack(1)//单字节对齐(Qt似乎是默认4字节对齐)
typedef struct{
    uint16_t Frame_heder;
    uint8_t  Caddr;
    uint8_t  Taddr[5];
    uint16_t Pwd;
    uint8_t  AFN;
    uint16_t Plen;
    uint8_t  SFlag;
    uint8_t  Serialnumber[2];
    uint8_t  Messagetime[6];
    uint8_t  message[1024];
    uint8_t  message_len;
}RTU_FrameInfo_s;
typedef union{
    RTU_FrameInfo_s message;
    uint8_t messageByte[2048];
}RTU_Message_s;

#pragma pack(pop)

class RTU_ParameterSetting
{
    public:
        RTU_ParameterSetting();
        ~RTU_ParameterSetting();
        BasicPage *m_basicPage;
        yunxingcanshu *m_runtimePage;
        sensor *m_sensorPage;
        portset *m_portset;
        MainWindow *m_MainWindow;
        Device_connection *m_Device_connection;
        dtuset  *m_dtuset;
        bool isbasedate;
        bool baseReadRetriedAfterLearn;
        QMutex  sendmessager_lock;
        QMutex  recivemessage_lock;
        RTU_Message_s m_sendmessage;
        RTU_Message_s m_recivemessage;
        uint32_t sMessageLen;
        uint32_t rMessageLen;
        s_FrameBaseInfo FrameBaseInfo;
        s_FrameBaseInfo displayInfo;
        void changemessagetodownmessage(uint8_t *src);
};
extern RTU_ParameterSetting *m_RTU_ParameterSetting;
void SwapDateByte(uint8_t *date,uint8_t n);
void Message_MainbadyEncode(RTU_Message_s *dateinfo,uint8_t AFN,int8_t sFlag,int16_t pLen,uint8_t mtype);
BYTE BCD2Hex(BYTE cBCD);
BYTE Hex2BCD(BYTE cHex);
uint16_t cal_crc16(BYTE* pData, uint32_t nLen);
uint8_t chartoascci(uint8_t date);
#endif // RTU_PARAMETERSETTING_H
