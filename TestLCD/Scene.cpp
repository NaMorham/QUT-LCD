
#include "stdafx.h"
#include <time.h>
#include <string>
#include <math.h>

#include "Scene.h"

#ifdef _WIN32
#define snprintf _snprintf
#endif

#ifndef M_PI
static const float PI = 3.14159265358979323846f;
#else
static const float PI = M_PI;
#endif

static const float GRAV = 1.0f;
static const float DIVISOR = 2.0f;
static const float DELTA_ANG = (PI / 180.0f)*5.0f;
static const float MAX_ANG = (PI / 180.0f)*80.0f;
static const float MIN_ANG = (PI / 180.0f)*10.0f;
static const float VEL_MODIFIER = 2.0f;
static const int MAX_VEL = 25;
static const int MIN_VEL = 1;

const float DegToRad(const float deg) { return (PI / 180.0F)*deg; }
const float RadToDeg(const float rad) { return (180.0F / PI)*rad; }

const int PIX_WIDTH = 7;
const int PIX_HEIGHT = 7;
const int PIX_SPACING = 1;

const int SCREEN_WIDTH = 84;
const int SCREEN_HEIGHT = 48;

const int NUM_ROWS = (SCREEN_HEIGHT / 8);

const int WND_WIDTH = (PIX_WIDTH * SCREEN_WIDTH) + 1; //((PIX_WIDTH + 0) * SCREEN_WIDTH) + 1 + PIX_WIDTH;
const int WND_HEIGHT = (PIX_HEIGHT * SCREEN_HEIGHT) + 1; //((PIX_HEIGHT + PIX_SPACING) * SCREEN_HEIGHT) + 1;

extern void SetPixel(const int x, const int y);
extern void ClearPixel(const int x, const int y);
extern void TogglePixel(const int x, const int y);
extern void SetByte(const byte c, const int x, const int y);
extern void SetChar(const char c, const int x, const int y);
extern void SetString(const char *str, const int x, const int y);
extern void SetByteArray(const byte *arr, const size_t sz, const int x, const int y);
extern void SetLine(const int x1, const int y1, const int x2, const int y2);
extern void SetLinePolar(const int x, const int y, const float ang, const int len);
extern void ClearBuffer();

static void CreateTerrain(byte *buf, const int sz);
static void DrawTank(const TankData *pTank);
static void ClearTank(const TankData *pTank);

static const byte tank[5] = { 0xc0, 0xc0, 0xe0, 0xc0, 0xc0 };
static int g_counter = 0;
//static time_t g_lastUpdate = 0;
//static time_t g_elapsedTime = 0;

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
static byte g_terrainBuf[SCREEN_WIDTH];

static TankData g_playerTank;
static ShellData g_playerShell;

static TankData g_aiTank;
static ShellData g_aiShell;
static BOOL g_playerWin = FALSE;

/*
extern double g_secondsPerCount;
extern double g_deltaTime;
extern __int64 g_prevTime;
extern __int64 g_currTime;
extern __int64 g_counterStart;
*/

extern const double GetSecondsSinceCounterStart();
extern void ResetCounter();


static const BOOL ShotInFlight(ShellData *pShell, const byte *pTerrain, const int terrainSz)
{
    // TODO, properly check terrain.  for now, just make sure shot is not in a tank or 
    return pShell->y < (float)(SCREEN_HEIGHT - 5);
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

static void Fire(TankData *pTank, ShellData *pShell, const float xMult)
{
    pShell->oldX = pShell->x = (double)pTank->x;
    pShell->oldY = pShell->y = (double)pTank->y;

    pShell->currentVelocityX = (double)pTank->launchVelocity * cos(pTank->launchAngle) * xMult / VEL_MODIFIER;
    pShell->currentVelocityY = -((double)pTank->launchVelocity * sin(pTank->launchAngle) / VEL_MODIFIER);       // remember screen y is opposite to world y
}

static void UpdateShell(ShellData *pShell, const double deltaTime)
{
    // preserve old Position
    pShell->oldX = pShell->x;
    pShell->oldY = pShell->y;

    pShell->x = pShell->x + (pShell->currentVelocityX * deltaTime / DIVISOR);
    while (pShell->x > (float)SCREEN_WIDTH)
    {
        pShell->x -= (float)SCREEN_WIDTH;
    }
    while (pShell->x < 0.0f)
    {
        pShell->x += SCREEN_WIDTH;
    }
    pShell->y = pShell->y + (pShell->currentVelocityY * deltaTime / DIVISOR);
    pShell->currentVelocityY += (GRAV * deltaTime / DIVISOR);
}

void InitScene()
{
    float fl = 0.0F;

    CreateTerrain(g_terrainBuf, 84);
//    g_lastUpdate = time(NULL);
//    g_elapsedTime = 0;
    
    btn_A_Down = btn_B_Down = FALSE;
    led_A_Active = led_B_Active = FALSE;

    g_playerTank.launchAngle = DegToRad(45.0F);
    g_aiTank.launchAngle = DegToRad(45.0F);
    g_aiTank.launchVelocity = g_playerTank.launchVelocity = 10;
    g_aiTank.y = g_playerTank.y = 40;
    g_aiTank.x = SCREEN_WIDTH-5;
    g_playerTank.x = 5;

    ClearBuffer();
}

static void DrawDefaultBase()
{
    ClearBuffer();

    SetByteArray(g_terrainBuf, 84, 0, 43);
    DrawTank(&g_playerTank);
    DrawTank(&g_aiTank);
}

static void DrawSceneTest()
{
    ClearBuffer();

    SetPixel(1, 1);
    SetPixel(1, 5);
    SetPixel(10, 10);
    SetChar('H', 15, 3);
    SetChar('I', 21, 3);
    SetString("This\0is a test string\0to see how\nbadly it breaks", 0, 20);
    SetByteArray(g_terrainBuf, 84, 0, 43);

    SetLine(35, 2, 35, 2);
    SetLine(37, 2, 37, 7);
    SetLine(39, 2, 44, 7);
    SetLine(46, 7, 51, 2);
    SetLine(42, 2, 48, 2);

    SetLine(53, 2, 58, 3);
    SetLine(53, 5, 58, 7);
    SetLine(53, 8, 58, 11);
    SetLine(53, 11, 58, 15);
    SetLine(53, 14, 58, 19);
    SetLine(53, 17, 58, 26);
    SetLine(53, 22, 58, 36);

    /*
    SetLine(60, 2, 65, 9);
    SetLine(60, 4, 65, 12);

    SetLine(82, 2, 67, 9);
    SetLine(82, 4, 67, 20);
    SetLine(82, 7, 67, 40);
    */

    {
        int startY = 2;
        int deltaY = 1;
        int endY = startY + deltaY;
        int xVal = 60;
        int idx = 0;

        for (; idx < 10; ++idx)
        {
            SetLine(xVal, startY, xVal + 5, endY);
            startY += 3;
            ++deltaY;
            endY = startY + deltaY;
        }
        xVal = 67;
        startY = 2;
        for (idx = 0; idx < 1; ++idx)
        {
            SetLine(xVal, startY, xVal + 5, endY);
            startY += 10;
            ++deltaY;
            endY = startY + deltaY;
        }
        xVal = 70;
        startY = 2;
        for (idx = 0; idx < 1; ++idx)
        {
            SetLine(xVal, startY, xVal + 5, endY);
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
    diff = (SCREEN_WIDTH - width) / 2;
    SetString(name, diff, 10);

    len = strlen(num);
    width = (len * 5) + (len - 1);
    diff = (SCREEN_WIDTH - width) / 2;
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
    diff = (SCREEN_WIDTH - width) / 2;
    SetString(name, diff, 10);

    if (g_counter > 0)
    {
        memset(buf, 0, sizeof(char) * 16);
        _snprintf(buf, 9, "%d", g_counter);
        len = strlen(buf);
        width = (len * 5) + (len - 1);
        diff = (SCREEN_WIDTH - width) / 2;
        SetString(buf, diff, 18);
    }
}

static void DrawStart()
{
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[16];

    memset(buf, 0, sizeof(char) * 16);
    _snprintf(buf, 9, "%d", g_counter);
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (SCREEN_WIDTH - width) / 2;
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
    diff = (SCREEN_WIDTH - width) / 2;
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
    diff = (SCREEN_WIDTH - width) / 2;
    SetString(buf, diff, 0);
#endif

    SetLinePolar(pTank->x, pTank->y, (float)-pTank->launchAngle, (int)(pTank->launchVelocity / VEL_MODIFIER));
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
    _snprintf(buf, 15, "[%.2f, %.2f]", pShell->x, pShell->y);
    len = strlen(buf);
    width = (len * 5) + (len - 1);
    diff = (SCREEN_WIDTH - width) / 2;
    SetString(buf, diff, 0);
#endif

    x = (int)pShell->oldX;
    y = (int)pShell->oldY;

    // clear old shell
    ClearPixel(x, y);
    ClearPixel(x+1, y);
    ClearPixel(x, y+1);
    ClearPixel(x+1, y+1);

    // draw new shell
    x = (int)pShell->x;
    y = (int)pShell->y;
    SetPixel(x, y);
    SetPixel(x + 1, y);
    SetPixel(x, y + 1);
    SetPixel(x + 1, y + 1);
}

static void DrawFinish()
{
    size_t width = 0;
    size_t diff = 0;
    size_t len = 0;
    char buf[32];

    memset(buf, 0, sizeof(char) * 32);
    len = snprintf(buf, 31, "Game Over");
    width = (len * 5) + (len - 1);
    diff = (SCREEN_WIDTH - width) / 2;
    SetString(buf, diff, 2);

    memset(buf, 0, sizeof(char) * 32);
    len = snprintf(buf, 31, "You %s", (g_playerWin ? "win!" : "lose."));
    width = (len * 5) + (len - 1);
    diff = (SCREEN_WIDTH - width) / 2;
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
        DrawTest();
        break;
    case STATE_P1_SHOT:
        DrawDefaultBase();
        DrawShell(&g_playerShell);
        break;
    case STATE_AI:
        DrawDefaultBase();
        DrawTest();
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

static void CreateTerrain(byte *buf, const int sz)
{
    int idx = 0;

    for (idx = 0; idx < sz; idx += 2)
    {
        buf[idx] = 0x01 | 0xaa;
        buf[idx + 1] = 0x01 | 0x55;
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
        if (btn_B_Down)
        {
            g_gameState = STATE_P1_VEL;
            btn_A_Down = btn_B_Down = FALSE;
        }
        break;
    case STATE_P1_VEL:
        if (btn_B_Down)
        {
            g_gameState = STATE_P1_FIRE;
            btn_A_Down = btn_B_Down = FALSE;
        }
        break;
    case STATE_P1_FIRE:
        if (btn_B_Down)
        {
            g_gameState = STATE_P1_ANG;
            btn_A_Down = btn_B_Down = FALSE;
        }
        else if (btn_A_Down)
        {
            g_gameState = STATE_P1_SHOT;
            g_counter = 0;
            ResetCounter();
            Fire(&g_playerTank, &g_playerShell, 1.0F);
        }
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
        else if (!ShotInFlight(&g_playerShell, g_terrainBuf, SCREEN_WIDTH))
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
        else if (!ShotInFlight(&g_aiShell, g_terrainBuf, SCREEN_WIDTH))
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
        if (btn_A_Down || btn_B_Down)
        {
            // reset game
            g_gameState = STATE_TITLE;
            g_counter = 0;
            ResetCounter();
            btn_A_Down = btn_B_Down = FALSE;
            led_A_Active = led_B_Active = FALSE;
        }
        else if (GetSecondsSinceCounterStart() >= 0.5)
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
        if (btn_A_Down)
        {
            btn_A_Down = FALSE;
            g_playerTank.launchAngle += DELTA_ANG;
            if (g_playerTank.launchAngle > MAX_ANG)
            {
                g_playerTank.launchAngle = MIN_ANG;
            }
        }
        break;
    case STATE_P1_VEL:
        if (btn_A_Down)
        {
            btn_A_Down = FALSE;
            g_playerTank.launchVelocity++;
            if (g_playerTank.launchVelocity > MAX_VEL)
            {
                g_playerTank.launchVelocity = MIN_VEL;
            }
        }
        break;
    case STATE_P1_FIRE:
    case STATE_P1_SHOT:
    case STATE_AI:
    case STATE_AI_SHOT:
    case STATE_FINISH:
    default:
        ;
    };
}
