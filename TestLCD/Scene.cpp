
#include "stdafx.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "TestLCD.h"
#include "Scene.h"

//#ifdef _WIN32
//#define snprintf _snprintf
//#endif

#ifndef M_PI
static const double PI = 3.14159265358979323846;
#else
static const double PI = M_PI;
#endif

static const double GRAV = 1.0;
static const double DIVISOR = 1.0;
static const double DELTA_ANG = (PI / 180.0f)*5.0;
static const double MAX_ANG = (PI / 180.0f)*80.0;
static const double MIN_ANG = (PI / 180.0f)*10.0;
static const double VEL_MODIFIER = 1.0;
static const int MAX_VEL = 25;
static const int MIN_VEL = 1;

const double DegToRad(const double deg) { return (PI / 180.0)*deg; }
const double RadToDeg(const double rad) { return (180.0 / PI)*rad; }

const int PIX_WIDTH = 7;
const int PIX_HEIGHT = 7;
const int PIX_SPACING = 1;

const int NUM_ROWS = (LCD_Y / 8);

const int WND_WIDTH = (PIX_WIDTH * LCD_X) + 1; //((PIX_WIDTH + 0) * LCD_X) + 1 + PIX_WIDTH;
const int WND_HEIGHT = (PIX_HEIGHT * LCD_Y) + 1; //((PIX_HEIGHT + PIX_SPACING) * LCD_Y) + 1;

// functions from the lcd
//extern void LCDSetPixel(const int x, const int y);
//extern void LCDClearPixel(const int x, const int y);
//extern void LCDTogglePixel(const int x, const int y);
//extern void LCDSetByte(const byte c, const int x, const int y);
//extern void LCDSetChar(const char c, const int x, const int y);
//extern void LCDSetString(const char *str, const int x, const int y);
//extern void LCDSetByteArray(const byte *arr, const size_t sz, const int x, const int y);
//extern void LCDSetLine(const int x1, const int y1, const int x2, const int y2);
//extern void LCDSetLinePolar(const int x, const int y, const float ang, const int len);
//extern void LCDClearBuffer();
//extern const int CalcOffsetAndShift(const int x, const int y, unsigned char *offset, unsigned char *shift);
//extern byte *GetScreenBuffer();

// local funcs
static void CreateTerrain(byte *buf, const int sz);
static void CreateDefaultTerrain(byte *buf, const int sz);
static void DrawTank(const TankData *pTank);
static void ClearTank(const TankData *pTank);
static void PopulateTerrainScreen();

static const byte tank[5] = { 0xc0, 0xc0, 0xe0, 0xc0, 0xc0 };

BOOL btn_A_Down = FALSE;
BOOL btn_B_Down = FALSE;
BOOL led_A_Active = FALSE;
BOOL led_B_Active = FALSE;

static const int STATE_INIT     = 0;
static const int STATE_TITLE    = 1;
static const int STATE_START    = 2;
static const int STATE_P1_ANG   = 3;
static const int STATE_P1_VEL   = 4;
static const int STATE_P1_FIRE  = 5;
static const int STATE_P1_SHOT  = 6;
static const int STATE_AI       = 7;
static const int STATE_AI_SHOT  = 8;
static const int STATE_FINISH   = 9;

static const int NUM_STATES     = 10;

const char *STATE_STRINGS[] = {
    "STATE_INIT",
    "STATE_TITLE",
    "STATE_START",
    "STATE_P1_ANG",
    "STATE_P1_VEL",
    "STATE_P1_FIRE",
    "STATE_P1_SHOT",
    "STATE_AI",
    "STATE_AI_SHOT",
    "STATE_FINISH",
    NULL
};

int g_gameState = STATE_INIT;
static int g_counter = 0;
static int g_turnCount = 0;
static double g_windStr = 0.0;    // +ve is to the right

static byte g_terrainBuf[LCD_X];
static const size_t SCREEN_BUFFER_COUNT = LCD_X*NUM_ROWS;
static byte g_terrainScreen[SCREEN_BUFFER_COUNT];     // Store the generated terrain. 
static const size_t SCREEN_BUFFER_SIZE = sizeof(byte)*SCREEN_BUFFER_COUNT;

static TankData g_playerTank;
static ShellData g_playerShell;

static TankData g_aiTank;
static ShellData g_aiShell;
static BOOL g_playerWin = FALSE;


const int GetMax(const int a, const int b)
{
    return (a > b ? a : b);
}

const int GetMin(const int a, const int b)
{
    return (a < b ? a : b);
}

const int Limit(const int val, const int minVal, const int maxVal)
{
    return GetMax(GetMin(val, maxVal), minVal);
}

const int RoundToInt(const double val)
{
    return (int)floor(val + 0.5);
}

extern const double GetSecondsSinceCounterStart();
extern void ResetCounter();

const int Rand(const int minVal, const int maxVal)
{
    double ratio = (double)rand() / (double)(RAND_MAX+1);
    int delta = maxVal - minVal;
    return (int)floor(((double)delta * ratio) + 0.5) + minVal;
}

static const BOOL ShotInFlight(ShellData *pShell, const byte *pTerrain, const int terrainSz)
{
    int x1 = RoundToInt(pShell->x);
    int x2 = (x1 + 1) % LCD_X;
    int ht = LCD_Y - (RoundToInt(pShell->y) + 1);

    return (ht > g_terrainBuf[x1]) && (ht > g_terrainBuf[x2]);
}

static const BOOL IsHit(TankData *pTank, ShellData *pShell)
{
    // because the shell is drawn as a 2x2 with +1x and +1 y
    // compare against tank pos.x - 3 and +2
    if ((pShell->x < (pTank->x - 3)) || (pShell->x > (pTank->x + 2)))
    {
        return false;
    }
    if ((pShell->y < (pTank->y - 2)) || (pShell->y >(pTank->y + 2)))
    {
        return false;
    }
    return true;
}

static void AdjustAITarget()
{
    double deltaAng = 0.0;
    int adj = 0;

    if (g_turnCount < 2)
    {
        return;     // AI has not fired back this game
    }

    BOOL lengthen = FALSE;
    if ((g_aiShell.x > g_playerTank.x) && !g_aiShell.wrapped)
    {
        lengthen = TRUE;
    }

    if (g_turnCount % 2)
    {
        deltaAng = RadToDeg(g_aiTank.launchAngle - DegToRad(45.0)) / 5.0;
        // adjust angle
        if (lengthen)
        {
            // calc towards 45% to lengthen
            adj = Rand(1, (int)fabs(deltaAng));
            if (deltaAng < 0.0)
            {
                g_aiTank.launchAngle += DegToRad(adj * 5.0);
            }
            else
            {
                g_aiTank.launchAngle -= DegToRad(adj * 5.0);
            }
        }
        else
        {
            // calc up or down away from 45 to shorten
            adj = Rand(1, 15) - 8;
            g_aiTank.launchAngle += DegToRad(adj * 5.0);
        }
    }
    else
    {
        // adjust power
        if (lengthen)
        {
            ++g_aiTank.launchVelocity;
        }
        else
        {
            --g_aiTank.launchVelocity;
        }
        if (g_aiTank.launchVelocity < MIN_VEL)
        {
            g_aiTank.launchVelocity = MAX_VEL;
        }
        else if (g_aiTank.launchVelocity > MAX_VEL)
        {
            g_aiTank.launchVelocity = MIN_VEL;
        }
    }
}

static void Fire(TankData *pTank, ShellData *pShell, const float xMult)
{
    pShell->oldX = pShell->x = (double)pTank->x;
    pShell->oldY = pShell->y = (double)pTank->y;

    pShell->wrapped = FALSE;

    pShell->currentVelocityX = (double)pTank->launchVelocity * cos(pTank->launchAngle) * xMult / VEL_MODIFIER;
    pShell->currentVelocityY = -((double)pTank->launchVelocity * sin(pTank->launchAngle) / VEL_MODIFIER);       // remember screen y is opposite to world y

    ++g_turnCount;
}

static void UpdateShell(ShellData *pShell, const double deltaTime)
{
    // preserve old Position
    pShell->oldX = pShell->x;
    pShell->oldY = pShell->y;

    pShell->x = pShell->x + (pShell->currentVelocityX * deltaTime / DIVISOR) + g_windStr;
    while (pShell->x > (float)LCD_X)
    {
        pShell->x -= (float)LCD_X;
        pShell->wrapped = TRUE;
    }
    while (pShell->x < 0.0f)
    {
        pShell->x += LCD_X;
        pShell->wrapped = TRUE;
    }
    pShell->y = pShell->y + (pShell->currentVelocityY * deltaTime / DIVISOR);
    pShell->currentVelocityY += (GRAV * deltaTime / DIVISOR);
}

void InitGame()
{
    memset(g_terrainScreen, 0, SCREEN_BUFFER_SIZE);
    memset(g_terrainBuf, 0, LCD_X);
    srand((unsigned int)time(NULL));
}

void CleanupGame()
{
    memset(g_terrainScreen, 0, SCREEN_BUFFER_SIZE);
    memset(g_terrainBuf, 0, LCD_X);
}

void InitScene()
{
    // new game
    CreateTerrain(g_terrainBuf, 84);
    ClearBuffer();
    PopulateTerrainScreen();
    
    // display vars
    btn_A_Down = btn_B_Down = FALSE;
    led_A_Active = led_B_Active = FALSE;

    // world vars
    g_turnCount = 0;
    g_windStr = (double)(Rand(0, 20)-10)/100.0;

    g_playerTank.x = 5;
    g_playerTank.y = LCD_Y - g_terrainBuf[g_playerTank.x] - 3;
    g_playerTank.launchAngle = DegToRad(45.0F);
    g_playerTank.launchVelocity = 10;

    g_aiTank.x = LCD_X - 5;
    g_aiTank.y = LCD_Y - g_terrainBuf[g_aiTank.x] - 3;
    g_aiTank.launchAngle = DegToRad((float)(Rand(1, 15) * 5 + 5));
    g_aiTank.launchVelocity = Rand(MIN_VEL, MAX_VEL);
}

static void DrawDefaultBase()
{
    byte *screenBuffer = NULL;

    ClearBuffer();

    //SetByteArray(g_terrainScreen, 84, 0, 43);
    screenBuffer = GetScreenBuffer();

    // copy the local terrain screen data in
    memcpy(screenBuffer, g_terrainScreen, SCREEN_BUFFER_SIZE);

    DrawTank(&g_playerTank);
    DrawTank(&g_aiTank);
}

static void DrawSceneTest()
{
    ClearBuffer();
    memcpy(GetScreenBuffer(), g_terrainScreen, SCREEN_BUFFER_SIZE);

    SetPixel(1, 1, 1);
    SetPixel(1, 5, 1);
    SetPixel(10, 10, 1);
    SetChar('H', 15, 3);
    SetChar('I', 21, 3);
    SetString("This\0is a test string\0to see how\nbadly it breaks", 0, 20);
    //SetByteArray(g_terrainScreen, 84, 0, 43);

    DrawLine(35, 2, 35, 2);
    DrawLine(37, 2, 37, 7);
    DrawLine(39, 2, 44, 7);
    DrawLine(46, 7, 51, 2);
    DrawLine(42, 2, 48, 2);

    DrawLine(53, 2, 58, 3);
    DrawLine(53, 5, 58, 7);
    DrawLine(53, 8, 58, 11);
    DrawLine(53, 11, 58, 15);
    DrawLine(53, 14, 58, 19);
    DrawLine(53, 17, 58, 26);
    DrawLine(53, 22, 58, 36);

    {
        int startY = 2;
        int deltaY = 1;
        int endY = startY + deltaY;
        int xVal = 60;
        int idx = 0;

        for (; idx < 10; ++idx)
        {
            DrawLine(xVal, startY, xVal + 5, endY);
            startY += 3;
            ++deltaY;
            endY = startY + deltaY;
        }
        xVal = 67;
        startY = 2;
        for (idx = 0; idx < 1; ++idx)
        {
            DrawLine(xVal, startY, xVal + 5, endY);
            startY += 10;
            ++deltaY;
            endY = startY + deltaY;
        }
        xVal = 70;
        startY = 2;
        for (idx = 0; idx < 1; ++idx)
        {
            DrawLine(xVal, startY, xVal + 5, endY);
            startY += 10;
            ++deltaY;
            endY = startY + deltaY;
        }
    }

    DrawTank(&g_playerTank);
}

static void DrawTitle()
{
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    static const char *name = "Andrew Hickey";
    static const char *num = "123456789";

    // width of text = (strlen() * 5) + (strlen()-1)
    len = strlen(name);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(name, diff, 10);

    len = strlen(num);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(num, diff, 20);
}

static void DrawTest()
{
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[16];
    const char *name = STATE_STRINGS[g_gameState];

    // width of text = (strlen() * 5) + (strlen()-1)
    len = strlen(name);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(name, diff, 10);

    if (g_counter > 0)
    {
        memset(buf, 0, sizeof(char) * 16);
        _snprintf(buf, 9, "%d", g_counter);
        len = strlen(buf);
        width = (len * 5) + (len - 1);
        diff = (LCD_X - width) / 2;
        SetString(buf, diff, 18);
    }
}

static void DrawStart()
{
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[16];

#ifdef _DEBUG
    memset(buf, 0, sizeof(char) * 16);
    _snprintf(buf, 15, "w = %.2f", g_windStr*100.0);
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 0);
#endif

    memset(buf, 0, sizeof(char) * 16);
    _snprintf(buf, 9, "%d", g_counter);
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 10);
}

static void DrawVelocity(TankData *pTank)
{
#ifdef _DEBUG
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[32];

    memset(buf, 0, sizeof(char) * 32);
    _snprintf(buf, 15, "(%d m/s)", pTank->launchVelocity);
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 0);
#endif

    SetLinePolar(pTank->x, pTank->y, (float)-pTank->launchAngle, (int)(pTank->launchVelocity / VEL_MODIFIER));
}

static void DrawAngle(TankData *pTank)
{
#ifdef _DEBUG
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[32];

    memset(buf, 0, sizeof(char) * 32);
    _snprintf(buf, 15, "(%.2f%c)", RadToDeg((float)pTank->launchAngle), 0x7f);
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 0);
#endif

    SetLinePolar(pTank->x, pTank->y, (float)-pTank->launchAngle, (int)(pTank->launchVelocity / VEL_MODIFIER));
}

static void DrawShot(TankData *pTank)
{
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[32];
    float ang = 0.0f;

#ifdef _DEBUG
    memset(buf, 0, sizeof(char) * 32);
    _snprintf(buf, 15, "%d@%.2f%c", pTank->launchVelocity, RadToDeg((float)pTank->launchAngle), 0x7f);
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 0);
#endif

    if (g_counter)
    {
        memset(buf, 0, sizeof(char) * 16);
        _snprintf(buf, 9, "%d", g_counter);
        len = strlen(buf);
        width = (len * 5) + (len - 1);
        diff = (LCD_X - width) / 2;
        SetString(buf, diff, 10);
    }

    if (pTank->x > 41)
    { // AI tank
        ang = (float)-(PI - pTank->launchAngle);
    }
    else
    { // Player tank
        ang = (float)-(pTank->launchAngle);
    }
    // transform the angle to suit AI facing left
    SetLinePolar(pTank->x, pTank->y, ang, (int)(pTank->launchVelocity / VEL_MODIFIER));
}

static void DrawShell(ShellData *pShell)
{
    int x = 0;
    int y = 0;

#ifdef _DEBUG
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[32];

    // x pos = 
    memset(buf, 0, sizeof(char) * 32);
    _snprintf(buf, 15, "[%.2f, ", pShell->x);
    len = strlen(buf);
    if (pShell->oldY > -10.0)
    {
        _snprintf(buf+len, 15-len, "%.2f]", (double)LCD_Y - pShell->y);
    }
    else if (pShell->oldY > -100.0)
    {
        _snprintf(buf + len, 15 - len, "%.1f]", (double)LCD_Y - pShell->y);
    }
    else
    {
        _snprintf(buf + len, 15 - len, "%d]", (double)LCD_Y - pShell->y);
    }
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 0);
#endif

    x = (int)pShell->oldX;
    y = (int)pShell->oldY;

    // clear old shell
    SetPixel(x, y, 0);
    SetPixel(x + 1, y, 0);
    SetPixel(x, y + 1, 0);
    SetPixel(x + 1, y + 1, 0);

    // draw new shell
    x = (int)pShell->x;
    y = (int)pShell->y;
    SetPixel(x, y, 1);
    SetPixel(x + 1, y, 1);
    SetPixel(x, y + 1, 1);
    SetPixel(x + 1, y + 1, 1);
}

static void DrawFinish()
{
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[32];

    memset(buf, 0, sizeof(char) * 32);
    len = _snprintf(buf, 31, "Game Over");
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 2);

    memset(buf, 0, sizeof(char) * 32);
    len = _snprintf(buf, 31, "You %s", (g_playerWin ? "win!" : "lose."));
    width = (len * 5) + (len - 1);
    diff = (LCD_X - width) / 2;
    SetString(buf, diff, 10);
}

void DrawScene()
{
    switch (g_gameState)
    {
    case STATE_INIT:
    case STATE_TITLE:
        /*
        DrawSceneTest();
        /*/
        DrawDefaultBase();
        DrawTitle();
        //*/
        break;
    case STATE_START:
        DrawDefaultBase();
        DrawStart();
        break;
    case STATE_P1_ANG:
        DrawDefaultBase();
        DrawAngle(&g_playerTank);
        break;
    case STATE_P1_VEL:
        DrawDefaultBase();
        DrawVelocity(&g_playerTank);
        break;
    case STATE_P1_FIRE:
        DrawDefaultBase();
        DrawShot(&g_playerTank);
        break;
    case STATE_P1_SHOT:
        DrawDefaultBase();
        DrawShell(&g_playerShell);
        break;
    case STATE_AI:
        DrawDefaultBase();
        DrawShot(&g_aiTank);
        break;
    case STATE_AI_SHOT:
        DrawDefaultBase();
        DrawShell(&g_aiShell);
        break;
    case STATE_FINISH:
        DrawDefaultBase();
        DrawFinish();
        break;
    default:
        DrawDefaultBase();
        DrawSceneTest();
    };
}

static void CreateDefaultTerrain(byte *buf, const int sz)
{
    int idx = 0;

    for (idx = 0; idx < sz; ++idx)
    {
        buf[idx] = 5;
    }
}

static void CreateBumpyTerrain(byte *buf, const int sz)
{
    int idx = 0;
    int oldY = 0;
    int newY = 0;
    int deltaY = 0;
    int baseLine = 0;
    int maxY = 0;
    int minY = 0;
    int x = 1;
    static const int min_y = 5;
    static const int max_y = 20;

    oldY = newY = baseLine = Rand(0, LCD_Y / 4) + min_y;
    maxY = GetMin(baseLine + 10, max_y);
    minY = GetMax(baseLine - 15, min_y);
    buf[0] = oldY;
    while (x < sz)
    {
        deltaY = Rand(0, 2) - 1;
        newY = buf[x - 1] + deltaY;
        newY = Limit(newY, minY, maxY);
        buf[x] = newY;
        ++x;
    }
}

static const int CalcVCHeight(byte *buf, const int sz)
{
    return 0;
}

static void CreateRollingTerrain(byte *buf, const int sz)
{
    int idx = 0;
    int oldY = 0;
    int newY = 0;
    int deltaX = 0;
    int deltaY = 0;
    int x = 0;
    static const int min_y = 5;

    oldY = newY = Rand(0, LCD_Y / 2) + min_y;

    // y = 1    01
    //          10
    //

    // y = 2    0001
    //          0110
    //          1000
    //

    // y = 3    0000001
    //          0000110
    //          0001000
    //          0110000
    //          1000000
    //

    // y = 4    00000001
    //          00000110
    //          00001000
    //          00110000
    //          11000000
    //

    // y = 5    000000001
    //          000000110
    //          000001000
    //          000010000
    //          001100000
    //          110000000
    //

    // y = 6    000000000000001
    //          000000000001110
    //          000000000110000
    //          000000001000000
    //          000000110000000
    //          000111000000000
    //          111000000000000
    //
    while (x < sz)
    {
        deltaY = Rand(0, 8) - 4;
        //  delta Y of:  1   2   3    4
        // req delta x:  1   4   8   16

        deltaX = Rand(0, 4) + deltaY;

        for (idx = 0; idx < deltaX; ++idx)
        {

        }

        ++x;
    }
}

static void CreateTerrain(byte *buf, const int sz)
{
    int sel = Rand(1, 5);

    switch (sel)
    {
    case 1:
    case 2:
        CreateBumpyTerrain(buf, sz);
        break;
    case 3:
        CreateDefaultTerrain(buf, sz);
        {
            size_t idx = 0;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 13;
            buf[LCD_X - idx] = buf[idx++] = 12;
            buf[LCD_X - idx] = buf[idx++] = 12;
            buf[LCD_X - idx] = buf[idx++] = 11;
            buf[LCD_X - idx] = buf[idx++] = 10;
            buf[LCD_X - idx] = buf[idx++] = 9;
            buf[LCD_X - idx] = buf[idx++] = 9;
            buf[LCD_X - idx] = buf[idx++] = 8;
            buf[LCD_X - idx] = buf[idx++] = 8;
            buf[LCD_X - idx] = buf[idx++] = 8;
            buf[LCD_X - idx] = buf[idx++] = 7;
            buf[LCD_X - idx] = buf[idx++] = 7;
            buf[LCD_X - idx] = buf[idx++] = 7;
            buf[LCD_X - idx] = buf[idx++] = 7;
            buf[LCD_X - idx] = buf[idx++] = 6;
            buf[LCD_X - idx] = buf[idx++] = 6;
            buf[LCD_X - idx] = buf[idx++] = 6;
            buf[LCD_X - idx] = buf[idx++] = 6;
            buf[LCD_X - idx] = buf[idx++] = 6;
        }
        break;
    default:
        CreateDefaultTerrain(buf, sz);
    }
}

static void DrawTank(const TankData *pTank)
{
    int idx = 0;
    byte val = 0;

    for (idx = 0; idx < 5; ++idx)
    {
        val = tank[idx];
        SetByte(val, pTank->x - 2 + idx, pTank->y - 5);
    }
}

static void ClearTank(const TankData *pTank)
{
    int idx = 0;
    byte val = 0;

    for (idx = 0; idx < 5; ++idx)
    {
        SetByte(val, pTank->x + idx-2, pTank->y - 5);
    }
}

void Update(const double deltaTime)
{
    switch (g_gameState)
    {
    case STATE_INIT:
    case STATE_TITLE:
        break;
    case STATE_START:
        if (g_counter <= 0)
        {
            g_gameState = STATE_P1_ANG;
            btn_A_Down = btn_B_Down = FALSE;
        }
        else if (GetSecondsSinceCounterStart() >= 1.0)
        {
            g_counter--;
            ResetCounter();
        }
        break;
    case STATE_P1_ANG:
    case STATE_P1_VEL:
    case STATE_P1_FIRE:
        break;
    case STATE_P1_SHOT:
        UpdateShell(&g_playerShell, deltaTime);
        if (g_counter && (g_playerShell.currentVelocityY >= 0) &&
            IsHit(&g_playerTank, &g_playerShell))
        {
            OutputDebugString("Player shot self\n");
            // player hit themselves
            led_B_Active = TRUE;
            led_A_Active = FALSE;
            g_playerWin = FALSE;
            g_gameState = STATE_FINISH;
        }
        else if (g_counter && IsHit(&g_aiTank, &g_playerShell))
        {
            OutputDebugString("Player shot AI\n");
            // player hit AI
            led_B_Active = FALSE;
            led_A_Active = TRUE;
            g_playerWin = TRUE;
            g_gameState = STATE_FINISH;
        }
        else if (!ShotInFlight(&g_playerShell, g_terrainScreen, LCD_X))
        {
            OutputDebugString("Player shell hit ground\n");
            g_counter = 3;
            ResetCounter();
            g_gameState = STATE_AI;
        }
        else
        {
            ++g_counter;
            ResetCounter();
        }
        break;
    case STATE_AI:
        if (g_counter <= 0)
        {
            g_counter = 0;
            ResetCounter();
            g_gameState = STATE_AI_SHOT;
            btn_A_Down = btn_B_Down = FALSE;
            AdjustAITarget();
            Fire(&g_aiTank, &g_aiShell, -1.0F);
        }
        else if (GetSecondsSinceCounterStart() >= 1.0)
        {
            g_counter--;
            ResetCounter();
        }
        break;
    case STATE_AI_SHOT:
        UpdateShell(&g_aiShell, deltaTime);
        if (g_counter && (g_aiShell.currentVelocityY >= 0) && 
            IsHit(&g_aiTank, &g_playerShell))
        {
            OutputDebugString("AI shot self\n");
            // AI hit themselves
            led_B_Active = FALSE;
            led_A_Active = TRUE;
            g_playerWin = TRUE;
            g_gameState = STATE_FINISH;
        }
        else if (g_counter && IsHit(&g_playerTank, &g_playerShell))
        {
            OutputDebugString("AI shot player\n");
            // AI hit player
            led_B_Active = TRUE;
            led_A_Active = FALSE;
            g_playerWin = FALSE;
            g_gameState = STATE_FINISH;
        }
        else if (!ShotInFlight(&g_aiShell, g_terrainScreen, LCD_X))
        {
            OutputDebugString("AI shell hit ground\n");
            g_counter = 0;
            ResetCounter();
            g_gameState = STATE_P1_ANG;
        }
        else
        {
            ++g_counter;
        }
        break;
    case STATE_FINISH:
        if (GetSecondsSinceCounterStart() >= 0.5)
        {
            // flash LEDs
            led_A_Active = led_B_Active;
            led_B_Active = !led_B_Active;
            ResetCounter();
        }
        break;
    default:
        ;
    };
}

void GetInput()
{
    switch (g_gameState)
    {
    case STATE_INIT:
    case STATE_TITLE:
        if (btn_A_Down || btn_B_Down)
        {
            g_gameState = STATE_START;
            g_counter = 3;
            ResetCounter();
            btn_A_Down = btn_B_Down = FALSE;
        }
        break;
    case STATE_START:
        break;
    case STATE_P1_ANG:
        if (btn_B_Down)
        {
            g_gameState = STATE_P1_VEL;
        }
        else if (btn_A_Down)
        {
            btn_A_Down = FALSE;
            g_playerTank.launchAngle += DELTA_ANG;
            if (g_playerTank.launchAngle > MAX_ANG)
            {
                g_playerTank.launchAngle = MIN_ANG;
            }
        }
        btn_A_Down = btn_B_Down = FALSE;
        break;
    case STATE_P1_VEL:
        if (btn_B_Down)
        {
            g_gameState = STATE_P1_FIRE;
        }
        else if (btn_A_Down)
        {
            btn_A_Down = FALSE;
            g_playerTank.launchVelocity++;
            if (g_playerTank.launchVelocity > MAX_VEL)
            {
                g_playerTank.launchVelocity = MIN_VEL;
            }
        }
        btn_A_Down = btn_B_Down = FALSE;
        break;
    case STATE_P1_FIRE:
        if (btn_B_Down)
        {
            g_gameState = STATE_P1_ANG;
        }
        else if (btn_A_Down)
        {
            g_gameState = STATE_P1_SHOT;
            g_counter = 0;
            ResetCounter();
            Fire(&g_playerTank, &g_playerShell, 1.0F);
        }
        btn_A_Down = btn_B_Down = FALSE;
        break;
    case STATE_P1_SHOT:
    case STATE_AI:
    case STATE_AI_SHOT:
    case STATE_FINISH:
        if (btn_A_Down || btn_B_Down)
        {
            // reset game
            g_gameState = STATE_TITLE;
            g_counter = 0;
            ResetCounter();
            btn_A_Down = btn_B_Down = FALSE;
            led_A_Active = led_B_Active = FALSE;
            InitScene();
        }
        break;
    default:
        ;
    };
}


#include <sstream>
#include <string>
#include <vector>
const char *BinString(const byte src, char *buf, const int bufSize)
{
    memset(buf, 0, bufSize * sizeof(char));
    byte val = src;
    int i = 0;
    for (; i < 8; ++i)
    {
        static const byte mask = 0x80;
        buf[i] = ((mask & val) == mask ? '1' : '0');
        val = val << 1;
    }
    return buf;
}

static void DumpBuffer(const byte *buf, const size_t sz)
{
    std::stringstream ss;
    size_t idx = 0;
    size_t bitIdx = 0;
    byte val = 0;
    char dbgBuf[9] = { 0 };
    std::vector<std::string> binStrings;

    OutputDebugString("\n");
    OutputDebugString("\n");
    for (idx = 0; idx < sz; ++idx)
    {
        val = buf[idx];
        BinString(val, dbgBuf, 9);
        binStrings.push_back(dbgBuf);
        OutputDebugString(dbgBuf);
        OutputDebugString("\n");
    }
    OutputDebugString("\n");

    size_t bit = 0;
    std::vector<std::string>::const_iterator itr = binStrings.begin();
    std::vector<std::string> rowStr;
    size_t bsSz = binStrings.size();
    for (bit = 0, itr; itr != binStrings.end(); ++bit, ++itr)
    {
        rowStr.push_back(*itr);
        // get a row full of strings
        if (rowStr.size() == LCD_X)
        {
            // dump to screen
            for (size_t cIdx = 0; cIdx < 8; ++cIdx)
            {
                std::vector<std::string>::const_iterator bItr = rowStr.begin();
                for (; bItr != rowStr.end(); ++bItr)
                {
                    const char c = bItr->c_str()[7-cIdx];
                    ss << c;
                }
                OutputDebugString(ss.str().c_str());
                OutputDebugString("\n");
                ss.str("");
            }
            rowStr.clear();
        }
    }
}

static void PopulateTerrainScreen()
{
    static const byte terrainFill[] = { 0xaa, 0x55 }; // 0xaa = 10101010   0x55 = 01010101
    byte val = 0;
    byte mask = 0x00;
    byte realHeight = 0;
    size_t idx = 0;
    size_t rowIdx = 0;
    unsigned char shift = 0;
    unsigned char offset = 0;
    size_t row = 0;
    size_t rowOffset = 0;
    
    memset(g_terrainScreen, 0, sizeof(byte)*LCD_X*NUM_ROWS);

    //loop through the terrain heights
    for (idx = 0; idx < LCD_X; ++idx)
    {
        mask = val = 0;
        realHeight = LCD_Y - g_terrainBuf[idx];

        // calc the row and offset values
        row = realHeight / 8; // how many bytes in real height
        CalcOffsetAndShift(idx, realHeight, &offset, &shift);
        val = terrainFill[idx % 2];

        for (rowIdx = NUM_ROWS-1; rowIdx > row; --rowIdx)
        {
            rowOffset = rowIdx * LCD_X;
            // fill any rows below 
            g_terrainScreen[rowOffset+idx] = val;
        }

        // fill the partial row 
        for (rowIdx = 0; rowIdx < shift; ++rowIdx)
        {
            mask |= (1 << rowIdx);
        }
        mask = ~mask;
        val = (val & mask);
        val |= (1 << shift);
        g_terrainScreen[offset] = val;
    }

    DumpBuffer(g_terrainScreen, SCREEN_BUFFER_COUNT);
}
