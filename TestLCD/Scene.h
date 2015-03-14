#ifndef __AMH__SCENE_H__
#define __AMH__SCENE_H__

#include "stdafx.h"
#include "ASCIIFont.h"
#include "graphics.h"

extern const int PIX_WIDTH;
extern const int PIX_HEIGHT;
extern const int PIX_SPACING;

extern const int NUM_ROWS;

extern const int WND_WIDTH;
extern const int WND_HEIGHT;

extern void DrawScene();
extern void GetInput();
extern void Update(const double deltaTime);
extern void InitScene();
extern void InitGame();
extern void CleanupGame();

extern BOOL btn_A_Down;
extern BOOL btn_B_Down;
extern BOOL led_A_Active;
extern BOOL led_B_Active;

const char *STATE_STRINGS[];
extern int g_gameState;

typedef struct
{
    int x;
    int y;
    int launchVelocity;
    double launchAngle;
} TankData;

typedef struct
{
    double x;
    double y;
    double oldX;
    double oldY;
    double currentVelocityX;
    double currentVelocityY;
    BOOL wrapped;
} ShellData;

#endif
