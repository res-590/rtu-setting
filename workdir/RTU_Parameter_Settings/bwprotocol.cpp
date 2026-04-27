#include "bwprotocol.h"
#include "mainwindow.h"
#include "QFile"
#include "QTextStream"
//extern void Base_BW(char *Base,int model,int *CJys,char *TPzb);


BWprotocol::BWprotocol()
{

}

UINT BWprotocol::Base_BW(char Base[], int model, char TPzb[],int straddr)
{
    QByteArray sendbuf;
    unsigned int nPos = 0;
    //EncodeBWhead(sendbuf,function);
    //Encode_Base(sendbuf,Base,model,CJ_data,TPzb,straddr);
    //unsigned sTemp = cal_crc16(pData,nPos);
    //pData[nPos++] = HIBYTE(sTemp);
    //pData[nPos++] = LOBYTE(sTemp);

    sendbuf[nPos++] = 0x7E;         //报文头
    sendbuf[nPos++] = 0x7E;

    sendbuf[nPos++] = 0x30;         //功能码

    sendbuf[nPos++] = 0x00;         //正文长度
    sendbuf[nPos++] = 0x00;

    sendbuf[nPos++] = 0x02;         //正文起始符

    sendbuf[nPos++] = 0xF0;         //正文，参数标识符F0 01
    sendbuf[nPos++] = 0x01;

    if(Base[0] != 0)                //中心站地址
    {
        sendbuf[nPos++] = Hex2BCD(Base[0]);
    }else
    {
        sendbuf[nPos++] = 0x10;
        sendbuf[nPos++] = 0x02;
    }
    if(Base[1] != 0)                //通信密码
    {
        sendbuf[nPos++] = Hex2BCD(Base[1]);
        sendbuf[nPos++] = 0x00;
    }else
    {
        sendbuf[nPos++] = 0x01;
        sendbuf[nPos++] = 0x00;
    }
    sendbuf[nPos++] = 0xF0;         //遥测站地址
    sendbuf[nPos++] = 0x03;
    sendbuf[nPos++] = straddr;
    sendbuf[nPos++] = 0xF0;         //工作模式
    sendbuf[nPos++] = 0x04;
    if(model)
    {
        sendbuf[nPos++] = Hex2BCD(model);
    }
    sendbuf[nPos++] = 0xF0;         //采集要素
    sendbuf[nPos++] = 0x05;
    for(int i=0;i<60;i++)
    {
        /*
        if(CJ_data[i] != 0)
        {
            sendbuf[nPos++] = CJ_data[i];

        }else
        {
            sendbuf[nPos++] = 0x00;
        }
        */
    }
    sendbuf[nPos++] = 0xF0;
    sendbuf[nPos++] = 0x06;
    for(int i=0;i<25;i++)          //图片自报整点
    {
        if(TPzb[i] != 0)
        {
            sendbuf[nPos++] = ASCII2Hex(TPzb[i]);
        }else
            break;
    }
    sendbuf[nPos++] = 0xF0;
    sendbuf[nPos++] = 0x03;

    return nPos;
}

UINT BWprotocol::yunxingcanshu_BW(unsigned char *pData, int CStype)
{
    (void)pData;
    (void)CStype;
    return 0;
}

UINT BWprotocol::sensor_BW(unsigned char *pData, int CStype)
{
    (void)pData;
    (void)CStype;
    return 0;
}

UINT BWprotocol::portset_BW(unsigned char *pData, int CStype)
{
    (void)pData;
    (void)CStype;
    return 0;
}

UINT BWprotocol::dtuset_BW(unsigned char *pData, int CStype)
{
    (void)pData;
    (void)CStype;
    return 0;
}
/*
//报文帧
UINT BWprotocol::EncodeBWhead(QByteArray pData, int function)
{

}

//基本参数设置报文
UINT BWprotocol::Encode_Base(QByteArray pData, char *Base, int model, QByteArray CJ_data, char *TPzb,int straddr)
{
    unsigned int nPos = 5;


    return *pData;
}
*/
//16位CRC计算
unsigned short BWprotocol::cal_crc16(unsigned char* pData,unsigned nLen)
{
    unsigned char j;
    unsigned short crc_reg = 0xFFFF;
    unsigned short current, i;

    for(i=0; i<nLen; i++)
    {
        current = pData[i];

        for(j=0; j<8; j++)
        {
            if((crc_reg^current) & 0x0001)
            {
                crc_reg = (crc_reg >> 1) ^ 0xa001;
            }
            else
            {
                crc_reg >>= 1;
            }

            current >>= 1;
        }
    }

    return crc_reg;
}


