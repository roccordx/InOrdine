/* Drawing coordinates are device-independent, converted when a frame is painted. */
#define C(x) (0xff000000u|(x))
#include "theme.c"
static const U tints[] = {C(0xe2edff), C(0xfbe9d3), C(0xe6e4fd), C(0xf6deea), C(0xffe6cf), C(0xe3e9ef), C(0xd8f1e7), C(0xe5ece3)};
static const U inks[] = {C(0x4572a4), C(0xac672e), C(0x7460a6), C(0x9f527c), C(0x9f682e), C(0x5b6e84), C(0x38826a), C(0x637969)};
static int dpi = 96;
static int px(int k) {
    return (k*dpi+(k >= 0 ? 48:-48))/96;
}
static int logical(int k) {
    return (k*96+dpi/2)/dpi;
}
static int clamp(int n, int lo, int hi) {
    return n<lo ? lo:(n>hi ? hi:n);
}
static int imax(int a, int b) {
    return a>b ? a:b;
}
static int imin(int a, int b) {
    return a<b ? a:b;
}
static H brush, family[3], fonts[13], fmt[6];
enum{F_TINY, F_SMALL, F_BODY, F_BOLD, F_CAP, F_HEADING, F_LOGO, F_NUMBER, F_MONO, F_FILE, F_DIM, F_EMPTY, F_STAT};
static const int fontSize[] = {11, 12, 14, 14, 11, 33, 26, 28, 12, 14, 13, 19, 13};
static const int fontFace[] = {1, 0, 0, 1, 1, 1, 1, 1, 2, 1, 0, 1, 1};
static void clearFonts(void) {
    for (int i = 0; i<COUNT(fonts); i++) {
        if (fonts[i]) GdipDeleteFont(fonts[i]);
        fonts[i] = 0;
    }
}
static void initArt(void) {
    GdipCreateSolidFill(WHITE, &brush);
    S names[] = {T("Segoe UI"), T("Segoe UI Semibold"), T("Consolas")};
    for (int j = 0; j<3; j++) if (GdipCreateFontFamilyFromName(names[j], 0, &family[j])) GdipGetGenericFontFamilySansSerif(&family[j]);
    for (int i = 0; i<6; i++) {
        GdipCreateStringFormat(i<3 ? 0x1000:0, 0, &fmt[i]);
        GdipSetStringFormatTrimming(fmt[i], 3);
        GdipSetStringFormatLineAlign(fmt[i], 1);
        GdipSetStringFormatAlign(fmt[i], i%3);
    }
}
static H beginG(H dc) {
    H g = 0;
    GdipCreateFromHDC(dc, &g);
    if (g) {
        GdipSetPageUnit(g, 2);
        GdipSetSmoothingMode(g, 4);
        GdipSetTextRenderingHint(g, 5);
    }
    return g;
}
static void col(U c) {
    GdipSetSolidFillColor(brush, c);
}
static H roundedPath(int xx, int yy, int ww, int hh, int r) {
    H p = 0;
    GdipCreatePath(0, &p);
    GdipAddPathArcI(p, xx, yy, r, r, 180.f, 90.f);
    GdipAddPathArcI(p, xx+ww-r, yy, r, r, 270.f, 90.f);
    GdipAddPathArcI(p, xx+ww-r, yy+hh-r, r, r, 0.f, 90.f);
    GdipAddPathArcI(p, xx, yy+hh-r, r, r, 90.f, 90.f);
    GdipClosePathFigure(p);
    return p;
}
static void box(H g, int x, int y, int w, int h, int rad, U c) {
    if (w <= 0 || h <= 0) return;
    col(c);
    int xx = px(x), yy = px(y), ww = px(x+w)-xx, hh = px(y+h)-yy, r = imin(px(2*rad), imin(ww, hh));
    if (r <= 1) {
        GdipFillRectangleI(g, brush, xx, yy, ww, hh);
        return;
    }
    H p = roundedPath(xx, yy, ww, hh, r);
    GdipFillPath(g, brush, p);
    GdipDeletePath(p);
}
static void card(H g, int x, int y, int w, int h, int r, U outer, U fill) {
    box(g, x, y, w, h, r, outer);
    box(g, x+1, y+1, w-2, h-2, r>1 ? r-1:0, fill);
}
static void dot(H g, int x, int y, int r, U c) {
    col(c);
    GdipFillEllipseI(g, brush, px(x-r), px(y-r), px(r*2), px(r*2));
}
static void polygon(H g, const Point *pts, int n, U c) {
    Point p[16];
    for (int i = 0; i<n; i++) {
        p[i].x = px(pts[i].x);
        p[i].y = px(pts[i].y);
    }
    col(c);
    GdipFillPolygonI(g, brush, p, n, 0);
}
static void line(H g, int x, int y, int xx, int yy, int w, U c) {
    H p = 0;
    GdipCreatePen1(c, (float) px(w), 2, &p);
    GdipDrawLineI(g, p, px(x), px(y), px(xx), px(yy));
    GdipDeletePen(p);
}
static void curve(H g, int x, int y, int xx, int yy, U c) {
    H pen = 0;
    int mid = (x+xx)/2;
    GdipCreatePen1(c, (float) px(2), 2, &pen);
    GdipDrawBezierI(g, pen, px(x), px(y), px(mid), px(y), px(mid), px(yy), px(xx), px(yy));
    GdipDeletePen(pen);
}
static void txt(H g, S s, int x, int y, int w, int h, int f, U c, int align) {
    if (!s || !s[0] || w <= 0 || h <= 0) return;
    if (!fonts[f]) GdipCreateFont(family[fontFace[f]], (float) px(fontSize[f]), 0, 2, &fonts[f]);
    if (!fonts[f]) return;
    RectF a = {(float) px(x), (float) px(y), (float)(px(x+w)-px(x)), (float)(px(y+h)-px(y))};
    col(c);
    GdipDrawString(g, s, -1, fonts[f], &a, fmt[align], brush);
}
static void folderIcon(H g, int x, int y, int s, U back, U front) {
    Point p[] = {{x, y+s/4}, {x+s/12, y+s/7}, {x+s*4/10, y+s/7}, {x+s*51/100, y+s*27/100}, {x+s*91/100, y+s*27/100}, {x+s, y+s*35/100}, {x+s, y+s*79/100}, {x, y+s*79/100}};
    polygon(g, p, COUNT(p), back);
    box(g, x, y+s*36/100, s, s*51/100, imax(1, s/9), front);
}
/* Eight teeth and a hollow hub. Coordinates use a 32-unit icon grid. */
static void gearIcon(H g, int x, int y, int size, U ink, U fill) {
    static const Point profile[] = {
        {14,1},{18,1},{19,5},{22,6},{26,4},{29,7},{27,11},{28,14},
        {32,15},{32,18},{28,19},{27,22},{29,26},{26,29},{22,27},{19,28},
        {18,32},{14,32},{13,28},{10,27},{6,29},{3,26},{5,22},{4,19},
        {0,18},{0,15},{4,14},{5,11},{3,7},{6,4},{10,6},{13,5}
    };
    Point poly[COUNT(profile)];
    for (int i=0; i<COUNT(poly); i++) {
        poly[i].x = px(x)+(px(size)*profile[i].x+16)/32;
        poly[i].y = px(y)+(px(size)*profile[i].y+16)/32;
    }
    col(ink);
    GdipFillPolygonI(g, brush, poly, COUNT(poly), 0);
    dot(g, x+size/2, y+size/2, imax(2,size/5), fill);
}
static void document(H g, int x, int y, int s, U color) {
    int w = s*72/100;
    int left = x+(s-w)/2;
    Point pp[] = {{left, y+1}, {left+w*64/100, y+1}, {left+w, y+s/4}, {left+w, y+s-1}, {left, y+s-1}};
    polygon(g, pp, 5, color);
    line(g, left+w/4, y+s*60/100, left+w*3/4, y+s*60/100, 1, WHITE);
    line(g, left+w/4, y+s*73/100, left+w*2/3, y+s*73/100, 1, WHITE);
}
enum{I_FOLDER, I_GRID, I_COPY, I_SPEED, I_DEMO, I_LIST, I_REFRESH, I_PLAY, I_STOP, I_CHEV, I_RETURN, I_ARROW, I_CHECK, I_ANIMATION};
static void glyph(H g, int t, int x, int y, int s, U c) {
    int m = s/2;
    if (t == I_FOLDER) {
        folderIcon(g, x, y, s, c, c);
        return;
    }
    if (t == I_ANIMATION) {
        int h = imax(3, s / 3);
        Point spark[] = {{x+m, y+1}, {x+m+2, y+m-2}, {x+s-1, y+m},
                         {x+m+2, y+m+2}, {x+m, y+s-1}, {x+m-2, y+m+2},
                         {x+1, y+m}, {x+m-2, y+m-2}};
        polygon(g, spark, COUNT(spark), c);
        dot(g, x+s-2, y+h-2, 1, c);
        return;
    }
    if (t == I_GRID) {
        int k = s*36/100;
        for (int j = 0; j<2; j++) for (int i = 0; i<2; i++) box(g, x+2+i*m, y+2+j*m, k, k, 2, c);
        return;
    }
    if (t == I_COPY) {
        box(g, x+2, y+2, s-6, s-5, 2, c);
        card(g, x+6, y+6, s-7, s-6, 2, c, NAV);
        return;
    }
    if (t == I_LIST) {
        for (int i = 0; i<3; i++) {
            dot(g, x+2, y+3+i*(s-5)/2, 1, c);
            line(g, x+6, y+3+i*(s-5)/2, x+s-1, y+3+i*(s-5)/2, 2, c);
        }
        return;
    }
    if (t == I_SPEED) {
        line(g, x+2, y+4, x+s-2, y+4, 2, c);
        line(g, x+5, y+m, x+s-2, y+m, 2, c);
        line(g, x+9, y+s-4, x+s-2, y+s-4, 2, c);
        return;
    }
    if (t == I_PLAY || t == I_DEMO) {
        Point p[] = {{x+s/4, y+2}, {x+s-2, y+m}, {x+s/4, y+s-2}};
        polygon(g, p, 3, c);
        return;
    }
    if (t == I_STOP) {
        box(g, x+3, y+3, s-6, s-6, 2, c);
        return;
    }
    if (t == I_CHEV) {
        line(g, x+2, y+m-2, x+m, y+m+2, 1, c);
        line(g, x+m, y+m+2, x+s-2, y+m-2, 1, c);
        return;
    }
    if (t == I_REFRESH || t == I_RETURN) {
        line(g, x+2, y+m, x+6, y+4, 2, c);
        line(g, x+6, y+4, x+s-3, y+4, 2, c);
        line(g, x+s-3, y+4, x+s-3, y+s-4, 2, c);
        line(g, x+s-3, y+s-4, x+6, y+s-4, 2, c);
        line(g, x+2, y+m, x+6, y+m+3, 2, c);
        return;
    }
    if (t == I_ARROW) {
        line(g, x+1, y+m, x+s-2, y+m, 2, c);
        line(g, x+s-7, y+m-5, x+s-2, y+m, 2, c);
        line(g, x+s-7, y+m+5, x+s-2, y+m, 2, c);
        return;
    }
    if (t == I_CHECK) {
        line(g, x+2, y+m, x+m-2, y+s-4, 2, c);
        line(g, x+m-2, y+s-4, x+s-2, y+3, 2, c);
    }
}
typedef struct {
    Str ext;
    H image;
} TypeIcon;
static TypeIcon iconCache[128];
static int iconCount, iconNext;
static H typeIcon(S ext) {
    for (int i = 0; i<iconCount; i++) if (eq(ext, iconCache[i].ext)) return iconCache[i].image;
    int k;
    if (iconCount<COUNT(iconCache)) k = iconCount++;
    else {
        k = iconNext++%COUNT(iconCache);
        drop(iconCache[k].ext);
        if (iconCache[k].image) GdipDisposeImage(iconCache[k].image);
    }
    iconCache[k].ext = dupw(ext);
    iconCache[k].image = 0;
    Str sample = join(T("file."), T(""), ext);
    SHFI info = {0};
    if (SHGetFileInfoW(sample, 0x80, &info, sizeof(info), 0x110) && info.icon) {
        GdipCreateBitmapFromHICON(info.icon, &iconCache[k].image);
        DestroyIcon(info.icon);
    }
    drop(sample);
    return iconCache[k].image;
}
static void fileArt(H g, Entry *e, int x, int y, int s) {
    H im = typeIcon(suffix(e->name));
    if (im) GdipDrawImageRectI(g, im, px(x), px(y), px(s), px(s));
    else document(g, x, y, s, inks[e->cat]);
}
static void brand(H g, int x, int y) {
    box(g, x, y, 38, 38, 10, MINT);
    for (int i = 0; i<3; i++) {
        int yy = y+11+6*i;
        line(g, x+9, yy, x+22-i*2, yy, 2, DARK);
        line(g, x+25-i*2, yy, x+29, yy, 2, GREEN);
    }
}
