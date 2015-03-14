// TestLCD.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

#include <math.h>

#include "TestLCD.h"
#include "ASCIIFont.h"
#include "Scene.h"

#define MAX_LOADSTRING 100

static const unsigned long pixCol = RGB(64, 64, 128);
static const unsigned long clearCol = RGB(220, 220, 255);

static const unsigned long ledRing = RGB(64, 0, 0);
static const unsigned long ledOn = RGB(255, 0, 0);
static const unsigned long ledOnHighlight = RGB(255, 128, 128);
static const unsigned long ledOff = RGB(128, 0, 0);
static const unsigned long ledOffHighlight = RGB(255, 0, 0);

const static DWORD PREV_KEY_MASK = (1 << 30);   // bit is set if key is down before message
const static DWORD PREV_KEY_STATE = (1 << 31);  // bit is set if key is being released

static byte *screenBuffer = NULL;
static int column = 0;
static int row = 0;
static int screenBufferIdx = 0;
static const int CONTROL_BUFFER = 30;

static void DrawPixel(HDC dc, HBRUSH brush, const int x, const int y);
static void DrawByteEx(HDC dc, HBRUSH onBrush, HBRUSH offBrush, const byte val, const int x, const int y);

static void DrawButton(HDC dc, RECT *pRect, BOOL active);
static void DrawButtons(HDC dc, RECT *controlRect);
static void DrawLED(HDC dc, RECT *pRect, BOOL active);
static void DrawLEDs(HDC dc, RECT *controlRect);
static void DrawControls(HDC dc);

static void Present(HDC hdc);

void SetPixel(const int x, const int y);
void ClearPixel(const int x, const int y);
void TogglePixel(const int x, const int y);
void SetByte(const byte c, const int x, const int y);
void SetChar(const char c, const int x, const int y);
void SetString(const char *str, const int x, const int y);
void SetByteArray(const byte *arr, const size_t sz, const int x, const int y);
void SetLine(const int x1, const int y1, const int x2, const int y2);
void SetLinePolar(const int x, const int y, const float ang, const int len);
void ClearBuffer();

static void CreateScreenBuffer(const int numCols, const int numRows);
static void FreeScreenBuffer();
static const BOOL CalcOffsetAndShift(const int x, const int y, size_t *offset, size_t *shift);

// GetInput, Update, and DrawScene come from external file scene.cpp

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HWND hWnd;


static double g_secondsPerCount = 0.0;
static double g_deltaTime = 0.0;
static __int64 g_prevTime = 0LL;
static __int64 g_currTime = 0LL;
static __int64 g_counterStart = 0LL;

const double GetSecondsSinceCounterStart()
{
    // Time difference between this frame and the previous.
    return (g_currTime - g_counterStart)*g_secondsPerCount;
}
void ResetCounter()
{
    g_counterStart = g_currTime;
}


// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPTSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    MSG msg = { 0 };
    HACCEL hAccelTable;

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_TESTLCD, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TESTLCD));

    // setup the timer
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    g_secondsPerCount = 1.0 / (double)countsPerSec;

    // Main message loop:
    while (msg.message != WM_QUIT)
    {
        if (g_deltaTime < 0.0)
        {
            g_deltaTime = 0.0;
        }

        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
        g_currTime = currTime;

        // Time difference between this frame and the previous.
        g_deltaTime = (g_currTime - g_prevTime)*g_secondsPerCount;

        // Prepare for next frame.
        g_prevTime = g_currTime;

        // If there are Window messages then process them.
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        // Otherwise, do animation/game stuff.
        else
        {
            if (1)
            {
                GetInput();
                Update(g_deltaTime);
                DrawScene();
            }
            else
            {
                Sleep(50);
            }
            OutputDebugString("Current state = \"");
            OutputDebugString(STATE_STRINGS[g_gameState]);
            OutputDebugString("\"\n");
            RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        }
    }

    return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TESTLCD));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_TESTLCD);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    //HWND hWnd;
    RECT r;

    hInst = hInstance; // Store instance handle in our global variable

    r.left = r.top = 0;
    r.right = WND_WIDTH;
    r.bottom = WND_HEIGHT + CONTROL_BUFFER;

    if (AdjustWindowRect(&r, (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX), TRUE) == FALSE)
    {
        r.left = CW_USEDEFAULT;
        r.top = 0;
        r.right = CW_USEDEFAULT;
        r.bottom = 0;
    }

    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        //      r.left, r.top, r.right-r.left, r.bottom-r.top, NULL, NULL, hInstance, NULL);
        CW_USEDEFAULT, 0, r.right - r.left, r.bottom - r.top, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

    CreateScreenBuffer(SCREEN_WIDTH, NUM_ROWS);
    InitScene();
    DrawScene();
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_COMMAND:
        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_KEYDOWN:
        switch (wParam)
        {
        case 'a':
        case 'A':
            btn_A_Down = TRUE;
            led_A_Active = TRUE;
            break;
        case 's':
        case 'S':
            btn_B_Down = TRUE;
            led_B_Active = TRUE;
            break;
        }
        RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        break;
    case WM_KEYUP:
        switch (wParam)
        {
        case 'a':
        case 'A':
            btn_A_Down = FALSE;
            led_A_Active = FALSE;
            break;
        case 's':
        case 'S':
            btn_B_Down = FALSE;
            led_B_Active = FALSE;
            break;
        }
        RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        {
            // TODO: Add any drawing code here...
            DrawControls(hdc);
            Present(hdc);
        }
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_QUIT:
        FreeScreenBuffer();
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

static void DrawPixel(HDC dc, HBRUSH brush, const int x, const int y)
{
    int xPos = 0;
    int yPos = 0;
    RECT r;

    // calc the actual postion
    xPos = x * PIX_WIDTH;
    yPos = y * PIX_HEIGHT;

    r.left = xPos + PIX_SPACING;
    r.top = yPos + PIX_SPACING;
    r.right = r.left + PIX_WIDTH - PIX_SPACING;
    r.bottom = r.top + PIX_HEIGHT - PIX_SPACING;

    FillRect(dc, &r, brush);
}

static void DrawByteEx(HDC dc, HBRUSH onBrush, HBRUSH offBrush, const byte val, const int x, const int y)
{
    int idx = 0;
    byte c = 0;
    byte bit = 0;
    static byte mask = 0x01;

    // loop through the val and 
    for (idx = 0; idx < 8; ++idx)
    {
        bit = 1 << idx;
        bit = val & bit;
        bit = bit >> idx;
        bit = bit & mask;

        if (bit)
        {
            DrawPixel(dc, onBrush, x, y + idx);
        }
        else
        {
            DrawPixel(dc, offBrush, x, y + idx);
        }
    }
}

static void DrawButton(HDC dc, RECT *pRect, BOOL active)
{

}

static void DrawButtons(HDC dc, RECT *controlRect)
{

}

static void DrawLED(HDC dc, RECT *pRect, BOOL active)
{
    HPEN oldPen = NULL;
    HBRUSH oldBrush = NULL;

    HPEN ringPen = CreatePen(PS_SOLID, 2, ledRing);
    HPEN ledPen = active ? CreatePen(PS_SOLID, 1, ledOn) : CreatePen(PS_SOLID, 1, ledOff);
    HPEN ledHighPen = active ? CreatePen(PS_SOLID, 1, ledOnHighlight) : CreatePen(PS_SOLID, 1, ledOffHighlight);
    HBRUSH ledBrush = CreateSolidBrush(active ? ledOn : ledOff);
    HBRUSH ledHighBrush = CreateSolidBrush(active ? ledOnHighlight : ledOffHighlight);

    oldPen = (HPEN)SelectObject(dc, ringPen);
    oldBrush = (HBRUSH)SelectObject(dc, ledBrush);

    Ellipse(dc, pRect->left, pRect->top, pRect->right, pRect->bottom);
    
    SelectObject(dc, ledPen);
    SelectObject(dc, ledHighBrush);

    Ellipse(dc, pRect->left+3, pRect->top+3, pRect->right-3, pRect->bottom-3);

    SelectObject(dc, ledBrush);

    Ellipse(dc, pRect->left + 5, pRect->top + 5, pRect->right - 4, pRect->bottom - 4);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    if (ledBrush)
    {
        DeleteObject(ledBrush);
        ledBrush = NULL;
    }
    if (ledHighBrush)
    {
        DeleteObject(ledHighBrush);
        ledHighBrush = NULL;
    }
    if (ringPen)
    {
        DeleteObject(ringPen);
        ringPen = NULL;
    }
    if (ledPen)
    {
        DeleteObject(ledPen);
        ledPen = NULL;
    }
    if (ledHighPen)
    {
        DeleteObject(ledHighPen);
        ledHighPen = NULL;
    }
}

static void DrawLEDs(HDC dc, RECT *controlRect)
{
    RECT leftLED = { 0 };
    RECT rightLED = { 0 };
    int LEDWidth = 0;

    leftLED.top = rightLED.top = controlRect->top + 2;
    leftLED.bottom = rightLED.bottom = controlRect->bottom - 2;
    leftLED.left = controlRect->left + 2;
    LEDWidth = (leftLED.bottom - leftLED.top);
    leftLED.right = leftLED.left + LEDWidth;
    rightLED.right = controlRect->right - 2;
    rightLED.left = rightLED.right - LEDWidth;

    DrawLED(dc, &leftLED, led_A_Active);
    DrawLED(dc, &rightLED, led_B_Active);
}

static void DrawControls(HDC dc)
{
    RECT client = { 0 };
    HBRUSH oldBrush = NULL;
    HBRUSH bkgBrush = CreateSolidBrush(GetSysColor(COLOR_MENU));
    HPEN oldPen = NULL;
    HPEN bkgPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_MENU));

    oldBrush = (HBRUSH)SelectObject(dc, bkgBrush);
    oldPen = (HPEN)SelectObject(dc, bkgPen);

    GetClientRect(hWnd, &client);
    client.top = client.bottom - CONTROL_BUFFER;

    FillRect(dc, &client, bkgBrush);

//    DrawLed
    DrawLEDs(dc, &client);
    DrawButtons(dc, &client);


// cleanup
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);

    if (bkgPen)
    {
        DeleteObject(bkgPen);
        bkgPen = NULL;
    }
    if (bkgBrush)
    {
        DeleteObject(bkgBrush);
        bkgBrush = NULL;
    }
}

static void Present(HDC dc)
{
    size_t idx = 0;
    size_t arraySz = SCREEN_WIDTH * NUM_ROWS;
    HPEN oldPen = 0;
    HPEN pen = CreatePen(PS_SOLID, 1, pixCol);
    HBRUSH onBrush = CreateSolidBrush(pixCol);
    HBRUSH offBrush = CreateSolidBrush(clearCol);

    oldPen = (HPEN)SelectObject(dc, pen);

    for (idx = 0; idx < arraySz; ++idx)
    {
        DrawByteEx(dc, onBrush, offBrush, screenBuffer[idx], idx % SCREEN_WIDTH, (idx / SCREEN_WIDTH) * 8);
    }

    // restore old objects
    SelectObject(dc, oldPen);

    DeleteObject(offBrush);
    DeleteObject(onBrush);
    DeleteObject(pen);
}

static void CreateScreenBuffer(const int numCols, const int numRows)
{
    if (screenBuffer != NULL)
    {
        MessageBox(NULL, "Screen buffer already allocated.", "Double Allocate", MB_OK | MB_ICONERROR);
        PostQuitMessage(1);
    }

    screenBuffer = (byte*)malloc(sizeof(byte)*numCols*numRows);

    if (screenBuffer == NULL)
    {
        MessageBox(NULL, "Failed to allocate screen buffer.", "Failed Allocate", MB_OK | MB_ICONERROR);
        PostQuitMessage(2);
    }
}

static void FreeScreenBuffer()
{
    if (screenBuffer == NULL)
    {
        MessageBox(NULL, "Screen buffer not allocated.", "Invalid free", MB_OK | MB_ICONERROR);
        return;
    }

    free(screenBuffer);
    screenBuffer = NULL;
}

static const BOOL CalcOffsetAndShift(const int x, const int y, size_t *offset, size_t *shift)
{
    if ((x < 0) || (x >= SCREEN_WIDTH) || (y < 0) || (y >= SCREEN_HEIGHT))
    {
        return FALSE;
    }
    if ((offset == NULL) || (shift == NULL))
    {
        return FALSE;
    }

    *offset = x + ((int)(y / 8))*SCREEN_WIDTH;
    *shift = y % 8;

    return TRUE;
}

//=============================================================================
void ClearBuffer()
{
    if (screenBuffer == NULL)
    {
        return;
    }

    memset(screenBuffer, 0, sizeof(char)*SCREEN_WIDTH*NUM_ROWS);
}

void SetPixel(const int x, const int y)
{
    size_t offset = 0;
    size_t shift = 0;
    byte val = 0;
    byte bit = 0x00;

    // find out where to set
    if (!CalcOffsetAndShift(x, y, &offset, &shift))
    {
        return;
    }

    // read the current value
    val = screenBuffer[offset];
    bit = (1 << shift);

    screenBuffer[offset] = val | bit;
}

void ClearPixel(const int x, const int y)
{
    size_t offset = 0;
    size_t shift = 0;
    byte val = 0;
    byte mask = 0x00;

    // find out where to set
    if (!CalcOffsetAndShift(x, y, &offset, &shift))
    {
        return;
    }

    // read the current value
    val = screenBuffer[offset];
    mask = ~(1 << shift);

    screenBuffer[offset] = val & mask;
}

void TogglePixel(const int x, const int y)
{
    size_t offset = 0;
    size_t shift = 0;
    byte val = 0;
    byte bit = 0x00;

    // find out where to set
    if (!CalcOffsetAndShift(x, y, &offset, &shift))
    {
        return;
    }

    // read the current value
    val = screenBuffer[offset];
    bit = (1 << shift);

    screenBuffer[offset] = val ^ bit;
}

void SetByte(const byte c, const int x, const int y)
{
    size_t bitIdx = 0;
    byte bit = 0;
    byte mask = 0x01;

    for (bitIdx = 0; bitIdx < 8; ++bitIdx)
    {
        bit = (c >> bitIdx) & mask;
        if (bit)
        {
            SetPixel(x, y + bitIdx);
        }
        else
        {
            ClearPixel(x, y + bitIdx);
        }
    }
}

void SetChar(const char c, const int x, const int y)
{
    size_t idx = 0;
    size_t bitIdx = 0;
    const unsigned char *chr = NULL;
    unsigned char ch = 0;
    byte bit = 0;
    byte mask = 0x01;

    chr = ASCII[c - ' '];

    for (idx = 0; idx < 5; ++idx)
    {
        ch = chr[idx];
        for (bitIdx = 0; bitIdx < 8; ++bitIdx)
        {
            bit = (ch >> bitIdx) & mask;
            if (bit)
            {
                SetPixel(x + idx, y + bitIdx);
            }
            else
            {
                ClearPixel(x + idx, y + bitIdx);
            }
        }
    }
}

void SetString(const char *str, const int x, const int y)
{
    size_t idx = 0;
    size_t len = 0;
    char c = 0;
    int xPos = 0;
    int yPos = y;

    if (str == NULL)
    {
        return;
    }
    if ((x >= SCREEN_WIDTH) || (y >= SCREEN_HEIGHT))
    {
        return;
    }
    len = strlen(str);

    for (xPos = x, idx = 0; idx < len; xPos += 6, ++idx)
    {
        c = str[idx];
        switch (c)
        {
        case '\n':
            yPos += 9;
        case '\r':
            xPos = x-6; // -6 to offset the fact we update xPos immediately
            break;
        default:
            SetChar(c, xPos, yPos);
        }
    }
}

void SetByteArray(const byte *arr, const size_t sz, const int x, const int y)
{
    size_t idx = 0;
    byte val = 0;

    for (idx = 0; idx < sz; ++idx)
    {
        val = arr[idx];
        SetByte(val, x + idx, y);
    }
}

void SetLine(const int x1, const int y1, const int x2, const int y2)
{
    int x_1(x1);
    int y_1(y1);
    int x_2(x2);
    int y_2(y2);
    int lastY = 0;
    int y = 0;
    int idx = 0;
    float grad = 0.0f;
    float y_intercept = 0.0f;

    // sort x and y to be in right order
    if (x_1 > x_2)
    {
        x_2 = x1;
        x_1 = x2;
        y_2 = y1;
        y_1 = y2;
    }
    lastY = y_1;

    if (x_1 == x_2)
    {
        if (y_1 == y_2)
        {
            SetPixel(x_1, y_1);
        }
        else if (y_1 < y_2)
        {
            for (idx = y_1; idx <= y_2; ++idx)
            {
                SetPixel(x_1, idx);
            }
        }
        else
        {
            for (idx = y_2; idx <= y_1; ++idx)
            {
                SetPixel(x_1, idx);
            }
        }
    }
    else
    {
        // calc gradient
        grad = ((float)(y_2 - y_1)) / ((float)(x_2 - x_1));
        // y = mx + c
        // c = y - mx
        y_intercept = y_2 - (grad * x_2);
        for (idx = x_1; idx <= x_2; ++idx)
        {
            y = (int)floor(((grad * idx) + y_intercept) + 0.5f);
            if (((y + 1) == lastY) || ((y - 1) == lastY) || (y == lastY))
            {
                SetPixel(idx, y);
            }
            else
            {
                //SetPixel(idx, y);
                SetLine(idx, (y<y_1 ? lastY-1 : lastY+1), idx, y);
            }
            lastY = y;
        }
    }
}

void SetLinePolar(const int x, const int y, const float ang, const int len)
{
    int x1 = x;
    int y1 = y;
    int x2 = x;
    int y2 = y;
    float sinAng = sinf(ang);
    float cosAng = cosf(ang);

    x2 = (int)floor((len * cosAng) + 0.5) + x;
    y2 = (int)floor((len * sinAng) + 0.5) + y;

    SetLine(x1, y1, x2, y2);
}
