#ifndef INORDINE_WIN_H
#define INORDINE_WIN_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t U;
typedef int32_t I;
typedef uint16_t W;
typedef uint8_t B;
typedef uint64_t U64;
typedef uintptr_t WP;
typedef intptr_t LP;
typedef intptr_t LR;
typedef void *H;
typedef W const*S;
typedef W *Str;
typedef struct {
    I x, y;
} Point;
typedef struct {
    I l, t, r, b;
} Rect;
typedef struct {
    float x, y, w, h;
} RectF;
typedef struct {
    U lo, hi;
} FT;
typedef struct {
    W year, month, dayOfWeek, day, hour, minute, second, ms;
} ST;
typedef struct {
    U attr;
    FT created, access, modified;
    U sizeHi, sizeLo, res0, res1;
    W name[260], alt[14];
} FindData;
typedef struct {
    U attr;
    FT created, access, modified;
    U sizeHi, sizeLo;
} Attr;
typedef struct {
    H hwnd;
    U msg;
    WP wp;
    LP lp;
    U time;
    Point point;
    U priv;
} Msg;
typedef struct {
    U size, style;
    LR(*proc)(H, U, WP, LP);
    I clsExtra, wndExtra;
    H instance, icon, cursor, background;
    S menuName, className;
    H iconSmall;
} WClass;
typedef struct {
    H dc;
    I erase;
    Rect rect;
    I restore, incUpdate;
    B reserved[32];
} Paint;
typedef struct {
    U type, id, item, action, state;
    H window, dc;
    Rect rect;
    WP data;
} DrawItem;
typedef struct {
    Point reserve, maxSize, maxPos, minTrack, maxTrack;
} MinMax;
typedef struct {
    H hwnd;
    WP id;
    U code;
} NM;
typedef struct {
    U mask;
    I item, subItem;
    U state, stateMask;
    Str text;
    I max, image;
    LP param;
    I indent, group;
    U nCols;
    U *cols;
    I*fmts;
    I iGroup;
} LVItem;
typedef struct {
    NM header;
    LVItem item;
} LVDisp;
typedef struct {
    U mask;
    I fmt, cx;
    Str text;
    I textMax, subItem, image, order, cxMin, cxDefault, cxIdeal;
} LVCol;
typedef struct {
    U flags;
    I iconIndex;
    U attrs;
    W display[260];
    W type[80];
} DummyShell;
typedef struct {
    H icon;
    I index;
    U attrs;
    W display[260], type[80];
} SHFI;
typedef struct {
    U size, classes;
} InitCC;
typedef struct {
    U version;
    H debug;
    I suppressThread, suppressCodecs;
} GPStart;
typedef struct {
    U a;
    W b, c;
    B d[8];
} Guid;
typedef struct {
    void **v;
} Com;
typedef struct {
    U attr;
    FT c, a, m;
    U volume, sizeHi, sizeLo, links, idHi, idLo;
} FileInfo;
typedef struct {
    B replace;
    H root;
    U nameBytes;
    W name[1];
} RenameInfo;
_Static_assert(sizeof(Msg) == 48 && sizeof(WClass) == 80 && sizeof(Paint) == 72 && sizeof(DrawItem) == 64, "Win64 UI ABI");
_Static_assert(sizeof(FindData) == 592 && sizeof(Attr) == 36 && sizeof(FileInfo) == 52, "Windows file ABI");
_Static_assert(sizeof(LVItem) == 88 && offsetof(LVDisp, item) == 24 && sizeof(SHFI) == 696, "Win64 shell ABI");
_Static_assert(offsetof(RenameInfo, root) == 8 && offsetof(RenameInfo, name) == 20, "Rename ABI");
#define BAD ((H)(intptr_t)-1)
#define WPTR ((H)(intptr_t)-1)
#define COUNT(x) ((int)(sizeof(x)/sizeof((x)[0])))
#define RES(x) ((S)(uintptr_t)(x))
#define WTEXT(x) ((S)L##x)
#define HI(x) (((U)(x)>>16)&65535)
#define LO(x) ((U)(x)&65535)
#define STATIC static
typedef struct {
    U size;
    H owner, instance;
    U result;
    U *custom;
    U flags;
    LP data;
    H hook;
    S templateName;
} ColorChoice;
_Static_assert(sizeof(ColorChoice)==72 && offsetof(ColorChoice, custom)==32, "CHOOSECOLORW ABI");
I ChooseColorW(ColorChoice*);
H LoadLibraryExW(S, H, U);
H GetProcAddress(H, const char*);
I SHGetKnownFolderPath(const Guid*, U, H, Str*);
/* KERNEL32 */
H GetProcessHeap(void);
H HeapAlloc(H, U, size_t);
H HeapReAlloc(H, U, H, size_t);
I HeapFree(H, U, H);
U GetLastError(void);
U GetModuleFileNameW(H, Str, U);
H GetModuleHandleW(S);
void ExitProcess(U);
U GetTickCount(void);
U GetFileAttributesW(S);
I GetFileAttributesExW(S, I, Attr*);
H FindFirstFileW(S, FindData*);
I FindNextFileW(H, FindData*);
I FindClose(H);
I CreateDirectoryW(S, H);
H CreateFileW(S, U, U, H, U, U, H);
I GetFileInformationByHandle(H, FileInfo*);
I SetFileInformationByHandle(H, I, const void*, U);
I CloseHandle(H);
U FormatMessageW(U, H, U, U, Str, U, H);
I CompareStringOrdinal(S, I, S, I, I);
I LCMapStringW(U, U, S, I, Str, I);
I FileTimeToSystemTime(const FT*, ST*);
I SystemTimeToTzSpecificLocalTime(H, const ST*, ST*);
H CreateThread(H, size_t, U(*)(H), H, U, U*);
H CreateEventW(H, I, I, S);
I SetEvent(H);
U WaitForSingleObject(H, U);
Str GetCommandLineW(void);
H LocalFree(H);
/* USER32 */
W RegisterClassExW(const WClass*);
H CreateWindowExW(U, S, S, U, I, I, I, I, H, H, H, H);
LR DefWindowProcW(H, U, WP, LP);
I ShowWindow(H, I);
I UpdateWindow(H);
I DestroyWindow(H);
I GetClientRect(H, Rect*);
I GetWindowRect(H, Rect*);
I InvalidateRect(H, const Rect*, I);
H BeginPaint(H, Paint*);
I EndPaint(H, const Paint*);
H GetDC(H);
I ReleaseDC(H, H);
I GetMessageW(Msg*, H, U, U);
I TranslateMessage(const Msg*);
LR DispatchMessageW(const Msg*);
void PostQuitMessage(I);
I PostMessageW(H, U, WP, LP);
LR SendMessageW(H, U, WP, LP);
H LoadCursorW(H, S);
H LoadIconW(H, S);
H LoadImageW(H, S, U, I, I, U);
I SetWindowTextW(H, S);
I EnableWindow(H, I);
H SetFocus(H);
H GetFocus(void);
I IsDialogMessageW(H, Msg*);
I IsWindow(H);
I MoveWindow(H, I, I, I, I, I);
I SetWindowPos(H, H, I, I, I, I, U);
U GetDpiForWindow(H);
U GetDpiForSystem(void);
I GetSystemMetrics(I);
I SystemParametersInfoW(U, U, H, U);
I AdjustWindowRectEx(Rect*, U, I, U);
WP SetTimer(H, WP, U, H);
I KillTimer(H, WP);
I MessageBoxW(H, S, S, U);
H CreatePopupMenu(void);
I AppendMenuW(H, U, WP, S);
U TrackPopupMenu(H, U, I, I, I, H, H);
I DestroyMenu(H);
I DrawFocusRect(H, const Rect*);
I FillRect(H, const Rect*, H);
I SetForegroundWindow(H);
H SendMessageFont_dummy(void);
/* GDI32 */
H CreateCompatibleDC(H);
H CreateCompatibleBitmap(H, I, I);
H SelectObject(H, H);
I DeleteObject(H);
I DeleteDC(H);
I BitBlt(H, I, I, I, I, H, I, I, U);
H CreateSolidBrush(U);
H CreateFontW(I, I, I, I, I, U, U, U, U, U, U, U, U, S);
/* GDIPLUS */
I GdiplusStartup(WP*, const GPStart*, H);
void GdiplusShutdown(WP);
I GdipCreateFromHDC(H, H*);
I GdipDeleteGraphics(H);
I GdipSetSmoothingMode(H, I);
I GdipSetTextRenderingHint(H, I);
I GdipSetPageUnit(H, I);
I GdipCreateSolidFill(U, H*);
I GdipSetSolidFillColor(H, U);
I GdipDeleteBrush(H);
I GdipSaveGraphics(H, U*);
I GdipRestoreGraphics(H, U);
I GdipTranslateWorldTransform(H, float, float, I);
I GdipRotateWorldTransform(H, float, I);
I GdipAddPathEllipseI(H, I, I, I, I);
I GdipCreatePathGradientFromPath(H, H*);
I GdipSetPathGradientCenterColor(H, U);
I GdipSetPathGradientSurroundColorsWithCount(H, const U*, I*);
I GdipCreateLineBrushI(const Point*, const Point*, U, U, I, H*);
I GdipDrawEllipseI(H, H, I, I, I, I);
I GdipCreatePen1(U, float, I, H*);
I GdipDeletePen(H);
I GdipDrawLineI(H, H, I, I, I, I);
I GdipDrawBezierI(H, H, I, I, I, I, I, I, I, I);
I GdipFillRectangleI(H, H, I, I, I, I);
I GdipFillEllipseI(H, H, I, I, I, I);
I GdipCreatePath(I, H*);
I GdipDeletePath(H);
I GdipAddPathLineI(H, I, I, I, I);
I GdipAddPathArcI(H, I, I, I, I, float, float);
I GdipClosePathFigure(H);
I GdipFillPath(H, H, H);
I GdipFillPolygonI(H, H, const Point*, I, I);
I GdipCreateFontFamilyFromName(S, H, H*);
I GdipGetGenericFontFamilySansSerif(H*);
I GdipDeleteFontFamily(H);
I GdipCreateFont(H, float, I, I, H*);
I GdipDeleteFont(H);
I GdipCreateStringFormat(I, W, H*);
I GdipSetStringFormatTrimming(H, I);
I GdipSetStringFormatLineAlign(H, I);
I GdipSetStringFormatAlign(H, I);
I GdipDeleteStringFormat(H);
I GdipDrawString(H, S, I, H, const RectF*, H, H);
I GdipCreateBitmapFromHICON(H, H*);
I GdipDisposeImage(H);
I GdipDrawImageRectI(H, H, I, I, I, I);
/* SHELL32 / COMCTL32 / OLE32 */
WP SHGetFileInfoW(S, U, SHFI*, U, U);
I DestroyIcon(H);
void DragAcceptFiles(H, I);
U DragQueryFileW(H, U, Str, U);
void DragFinish(H);
W **CommandLineToArgvW(S, I*);
I SHCreateItemFromParsingName(S, H, const Guid*, Com**);
I CoInitializeEx(H, U);
void CoUninitialize(void);
I CoCreateInstance(const Guid*, H, U, const Guid*, Com**);
void CoTaskMemFree(H);
I InitCommonControlsEx(const InitCC*);
/* ADVAPI32 */
I RegGetValueW(H, S, S, U, U*, H, U*);
I RegCreateKeyExW(H, S, U, Str, U, U, H, H*, U*);
I RegSetValueExW(H, S, U, U, const B*, U);
I RegCloseKey(H);
#endif
