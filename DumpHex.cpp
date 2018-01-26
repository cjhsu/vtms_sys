//---------------------------------------------------------------------------
#include <ctype.h>
#include <windows.h>
#pragma hdrstop

#include "DumpHex.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

char __HexTable__[] = "0123456789ABCDEF";

int DumpHex( char *buff, char *data, int size )
{
    char *pAsciiStr;
    unsigned char CurByte;
    int Len = 0;
    int Pos = 0;

    char *pHexStr = buff;

    for(int i=0; i<size; i++, pAsciiStr++)
    {
        Pos = i & 0xF;

        if(!Pos)
        {
            if(i)
            {
                pAsciiStr[0] = '\r';
                pAsciiStr[1] = '\n';
                pHexStr = pAsciiStr + 2;
            }

            pHexStr[0]   = __HexTable__[(i >> 16) & 0x0F];
            pHexStr[1]   = __HexTable__[(i >> 8)  & 0x0F];
            pHexStr[2]   = __HexTable__[(i >> 4)  & 0x0F];
            pHexStr[3]   = __HexTable__[i         & 0x0F];
            pHexStr[4]   = ':';
            pHexStr[5]   = ' ';
            pHexStr[6]   = ' ';
            pHexStr      += 7;
            //
            pAsciiStr    = pHexStr + 50;
            pAsciiStr[0] = ' ';
            pAsciiStr[1] = ' ';
            pAsciiStr    += 2;
        }

        CurByte = data[i];
        pHexStr[0] = __HexTable__[CurByte >> 4];
        pHexStr[1] = __HexTable__[CurByte & 0x0F];
        pHexStr[2] = ' ';
        pHexStr += 3;

        *pAsciiStr = isprint(CurByte) ? CurByte : '.';

        if(Pos == 7)
        {
            pHexStr[0]   = '|';
            pHexStr[1]   = ' ';
            pHexStr += 2;
            //*++pAsciiStr = ' ';
        }
    }

    Len += 77 * (size >> 4);
    int Rest = 0xF - Pos;
    if (Rest & 0x08)
        Rest++;
    if (Rest)
    {
        Len += 77;
        while(Rest--)
        {
            pHexStr[0] = ' ';
            pHexStr[1] = ' ';
            pHexStr[2] = ' ';
            pHexStr += 3;
            if (Rest != 7)
                *pAsciiStr++ = ' ';
        }
    }

    Len += 2;

    *(unsigned long*)pAsciiStr = 0x0D0A0D00; //'\r\n\r\0';
    pAsciiStr[4] = '\0';

    return Len;
}
//---------------------------------------------------------------------------
