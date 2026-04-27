#include "RTU_ParameterSetting.h"
#define LOOCTET(b)              ((BYTE)(((BYTE)(b)) &0x0f))
#define HIOCTET(b)              ((BYTE)((((BYTE)(b)) >>4) &0x0f))

RTU_ParameterSetting *m_RTU_ParameterSetting;
RTU_ParameterSetting::RTU_ParameterSetting()
{
    m_MainWindow = nullptr;
    m_Device_connection = nullptr;
    m_basicPage = nullptr;
    m_runtimePage = nullptr;
    m_sensorPage = nullptr;
    m_dtuset = nullptr;
    m_portset = nullptr;
    isbasedate = false;
    baseReadRetriedAfterLearn = false;
    memset(&FrameBaseInfo, 0, sizeof(FrameBaseInfo));
    memset(&displayInfo, 0, sizeof(displayInfo));
    FrameBaseInfo.centeraddr = 0x10;
    FrameBaseInfo.testaddr[0] = 0x00;
    FrameBaseInfo.testaddr[1] = 0x07;
    FrameBaseInfo.testaddr[2] = 0x56;
    FrameBaseInfo.testaddr[3] = 0x46;
    FrameBaseInfo.testaddr[4] = 0x30;
    FrameBaseInfo.cmcpassword = 0x0100;
    displayInfo = FrameBaseInfo;
    memset(m_sendmessage.messageByte,0,sizeof (m_sendmessage));
    memset(m_recivemessage.messageByte,0,sizeof (m_recivemessage));
    sMessageLen = 0;
    rMessageLen = 0;
}
RTU_ParameterSetting::~RTU_ParameterSetting()

{
    if(m_MainWindow) delete m_MainWindow;
    if(m_Device_connection) delete  m_Device_connection;
}
void RTU_ParameterSetting:: changemessagetodownmessage(uint8_t *src)
{
    char buff = *(src + 2);
    for(int i = 2; i < 7; i++){
        *(src + i) = *(src + i + 1);
    }
    *(src + 7) = buff;
}
BYTE BCD2Hex(BYTE cBCD)
{
    return HIOCTET(cBCD)*10+LOOCTET(cBCD);
}
BYTE Hex2BCD(BYTE cHex)
{
    return ((cHex/10)%10)*16 + cHex % 10;
}
/**
 * @brief 16位CRC计算
 */
uint16_t cal_crc16(BYTE* pData, uint32_t nLen)
{
    uint8_t j;
    uint16_t crc_reg = 0xFFFF;
    uint16_t current, i;
    for (i=0; i<nLen; i++) {
        current = (uint16_t)pData[i];
        for (j=0; j<8; j++) {
            if ((crc_reg^current) & 0x0001) {
                crc_reg = (crc_reg >> 1) ^ 0xa001;
            }
            else {
                crc_reg >>= 1;
            }
            current >>= 1;
        }
    }
    return crc_reg;
}

void SwapDateByte(uint8_t *date,uint8_t n)
{
    uint8_t temp;
    uint8_t head = 0,tail = n-1;
    while(head < tail){
        temp = *(date + head);
        *(date + head) = *(date + tail);
        *(date + tail) = temp;
        head++;
        tail--;
    }
}

void Message_MainbadyEncode(RTU_Message_s *dateinfo,uint8_t AFN,int8_t sFlag,int16_t pLen,uint8_t mtype)
{
    const bool hasFrameInfo =
        m_RTU_ParameterSetting->FrameBaseInfo.centeraddr != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[0] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[1] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[2] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[3] != 0 ||
        m_RTU_ParameterSetting->FrameBaseInfo.testaddr[4] != 0;
    const uint8_t defaultTaddr[5] = {0x00, 0x07, 0x56, 0x46, 0x30};

    dateinfo->message.Frame_heder = 0x7E7E;
    ///<这里因为是调试阶段先直接填充，否者从主界面抓取
    dateinfo->message.Caddr = hasFrameInfo ? m_RTU_ParameterSetting->FrameBaseInfo.centeraddr : 0x10;
    memcpy(dateinfo->message.Taddr,
           hasFrameInfo ? m_RTU_ParameterSetting->FrameBaseInfo.testaddr : defaultTaddr,
           5);
    dateinfo->message.Pwd = hasFrameInfo ? m_RTU_ParameterSetting->FrameBaseInfo.cmcpassword : 0x0100;
    SwapDateByte((uint8_t*) &(dateinfo->message.Pwd),sizeof(dateinfo->message.Pwd));
    dateinfo->message.AFN = AFN;
    dateinfo->message.SFlag = sFlag;
    ///<流水号应该是全局变量
    dateinfo->message.Serialnumber[0] = 0x00;
    dateinfo->message.Serialnumber[1] = 0x00;
    ///<获取当前时间
    time_t t = time(NULL);
    struct tm* info = localtime(&t);
    dateinfo->message.Messagetime[0] = Hex2BCD((info->tm_year + 1900) % 100);
    dateinfo->message.Messagetime[1] = Hex2BCD(info->tm_mon + 1);
    dateinfo->message.Messagetime[2] = Hex2BCD(info->tm_mday);
    dateinfo->message.Messagetime[3] = Hex2BCD(info->tm_hour);
    dateinfo->message.Messagetime[4] = Hex2BCD(info->tm_min);
    dateinfo->message.Messagetime[5] = Hex2BCD(info->tm_sec);
    dateinfo->message.Plen = pLen;
    if(mtype == MTYPE_DOWN){
        ///<下行报文报文上行标识及长度 高 4 位表示上下行标识 ，1000 表示下行
        dateinfo->message.Plen |= 0x8000;
        SwapDateByte((uint8_t*) &(dateinfo->message.Plen),sizeof(dateinfo->message.Plen));
        ///<下行报文的中心站地址在遥测站地址之后
        m_RTU_ParameterSetting->changemessagetodownmessage(dateinfo->messageByte);
    }
    else{
        ///<上行报文报文上行标识及长度 高 4 位表示上下行标识 ，0000 表示下行
        dateinfo->message.Plen &= 0x0fff;
        SwapDateByte((uint8_t*) &(dateinfo->message.Plen),sizeof(dateinfo->message.Plen));
    }
}
uint8_t chartoascci(uint8_t date)
{
    if(date >= '0' && date <= '9') date -= '0';
    else if(date >= 'a' && date <= 'z') {
        date -= 'a';
        date += 10;
    }
    else if(date >= 'A' && date <= 'Z') {
        date -= 'A';
        date += 10;
    }
    return  date;
}
