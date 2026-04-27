#ifndef BWPROTOCOL_H
#define BWPROTOCOL_H
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define JBCS 1
#define YXCS 2
#define CGQCS 3
#define NZDTU 4
#define DKCS 5

#include "QButtonGroup"

typedef unsigned int        UINT;
typedef unsigned char       BYTE;
#define MAKEBYTE(high, low)     ((BYTE)((((BYTE)(low)) &0x0f)) | ((((BYTE)(high)) &0xff) <<4))

[[maybe_unused]] static BYTE Hex2BCD(BYTE cHex)  //十六进制转BCD
{
    if(cHex > 99)
        return 0;
    return MAKEBYTE(cHex/10,cHex%10);
}

[[maybe_unused]] static BYTE ASCII2Hex(BYTE cASCII)  //ascii2转十六进制
{
    if((cASCII >= '0') && (cASCII <= '9'))
        return cASCII - '0';
    else if((cASCII >= 'A') && (cASCII <= 'F'))
        return cASCII-'A' + 0x0A;
    else if((cASCII >= 'a') && (cASCII <= 'f'))
        return cASCII-'a' + 0x0A;
    return 0;
}


class BWprotocol
{
public:
    BWprotocol();

    static UINT Base_BW(char Base[],int model,char TPzb[],int straddr);

    static UINT yunxingcanshu_BW(unsigned char* pData,int CStype);

    static UINT sensor_BW(unsigned char* pData,int CStype);

    static UINT portset_BW(unsigned char* pData,int CStype);

    static UINT dtuset_BW(unsigned char* pData,int CStype);

    //static UINT EncodeBWhead(QByteArray pData,int function);

    static unsigned short cal_crc16(unsigned char* pData,unsigned nLen);

    //static UINT Encode_Base(QByteArray pData,char *Base, int model, QByteArray CJ_data, char *TPzb,int straddr);

};

#endif // BWPROTOCOL_H
