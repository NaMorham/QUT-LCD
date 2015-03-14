#ifndef __AMH__TESTLCD_H__
#define __AMH__TESTLCD_H__

#include "resource.h"

extern unsigned char PORTA;
extern unsigned char PORTB;
extern unsigned char PORTC;
extern unsigned char PORTD;
extern unsigned char PORTE;
extern unsigned char PORTF;

extern unsigned char PINA;
extern unsigned char PINB;
extern unsigned char PINC;
extern unsigned char PIND;
extern unsigned char PINE;
extern unsigned char PINF;

extern unsigned char DDRA;
extern unsigned char DDRB;
extern unsigned char DDRC;
extern unsigned char DDRD;
extern unsigned char DDRE;
extern unsigned char DDRF;

extern unsigned int g_offset;

extern "C" 
{
    void LCDSetPixel(const int x, const int y);
    void LCDClearPixel(const int x, const int y);
    void LCDTogglePixel(const int x, const int y);
    void LCDSetByte(const unsigned char c, const int x, const int y);
    void LCDSetChar(const char c, const int x, const int y);
    void LCDSetString(const char *str, const int x, const int y);
    void LCDSetByteArray(const unsigned char *arr, const unsigned int sz, const int x, const int y);
    void LCDSetLine(const int x1, const int y1, const int x2, const int y2);
    void LCDSetLinePolar(const int x, const int y, const float ang, const int len);
    void LCDClearBuffer();
    void LCDBufferWrite(unsigned char dc, unsigned char data);

    unsigned char *GetScreenBuffer();
}

extern const int CalcOffsetAndShift(const int x, const int y, unsigned char *offset, unsigned char *shift);

//extern void LCDBufferWrite(unsigned char dc, unsigned char data);

#endif //__AMH__TESTLCD_H__
