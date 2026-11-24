enum{SCAN, READY, RUNNING, FINISHED};
enum{CHOOSE = 101, METHOD, CONFLICT, EFFECT, SPEED, DEMO, DETAIL, REFRESH, GO, GEAR, END_BUTTON};
typedef struct {
    int x, y, w, h;
} Area;
static H mainwin, inst, buttons[END_BUTTON-CHOOSE], details, lv, lvFont;
static Plan *plan;
static Str base, selfFile;
static int mode, conflict, speed, demo, phase = SCAN, cw = 1080, ch = 670;
static int doneCount, successCount, skipCount, errCount, flight = -1, stopping, stopped, closing;
enum { CLASSIC, BURST, ORBIT, EFFECT_COUNT };
static const int effectNames[] = {M_CLASSIC, M_BURST, M_ORBIT};
static int effect = BURST;
static int opening;
static U flightTime, openingTime, pickupElapsed, sceneEpoch, sceneSeed;
enum { MAX_SCENE_FILES = 20, ANIMATION_TIMER = 21 };
static int retainedFiles[MAX_SCENE_FILES], retainedCount;
static U openingDuration(void) {
    if (speed == 2 || effect == CLASSIC) return 0;
    if (effect == BURST) return speed == 1 ? 405 : 1270;
    return speed == 1 ? 300 : 940;
}
static U flightDuration(void) {
    if (speed == 2) return 0;
    if (effect == CLASSIC) return speed == 1 ? 145 : 545;
    return speed == 1 ? 175 : (effect == BURST ? 735 : 690);
}
static Str runError;
static int historyWarning;
static int side(void) {
    return cw<950 ? 222:252;
}
static int pad(void) {
    return cw<950 ? 20:30;
}
static int shortView(void) {
    return ch<585;
}
static int optsY(void) {
    return shortView() ? 166:195;
}
static int optionStep(void) {
    return shortView() ? 56 : 64;
}
static Area buttonArea(int id) {
    int sw = side(), p = pad(), x = sw+p, r = cw-p;
    int gearWidth = cw >= 995 ? 135 : 35;
    switch (id) {
        case CHOOSE:
        return (Area){20, shortView() ? 83:99, sw-40, 37};
        case METHOD:
        return (Area){20, optsY()+22, sw-40, shortView() ? 32 : 37};
        case CONFLICT:
        return (Area){20, optsY()+optionStep()+22, sw-40, shortView() ? 32 : 37};
        case EFFECT:
        return (Area){20, optsY()+2*optionStep()+22, sw-40, shortView() ? 32 : 37};
        case SPEED:
        return (Area){20, optsY()+3*optionStep()+22, sw-40, shortView() ? 32 : 37};
        case DEMO:
        return (Area){20, ch-58, sw-40, 38};
        case DETAIL:
        return (Area){r-gearWidth-111, 25, 104, 35};
        case REFRESH:
        return (Area){r-gearWidth-153, 25, 35, 35};
        case GEAR:
        return (Area){r-gearWidth, 25, gearWidth, 35};
        case GO:
        return (Area){r-169, ch-66, 169, 43};
        default:
        return (Area){x, 0, 0, 0};
    }
}
static S caption(int id) {
    switch (id) {
        case CHOOSE:
        return tr(M_CHOOSE);
        case METHOD:
        return tr(methodText[mode]);
        case CONFLICT:
        return tr(conflictText[conflict]);
        case EFFECT:
        return tr(effectNames[effect]);
        case SPEED:
        return tr(speedText[speed]);
        case DEMO:
        return demo ? tr(M_EXIT_DEMO):tr(M_TRY_DEMO);
        case DETAIL:
        return tr(M_DETAILS);
        case REFRESH:
        return tr(M_REFRESH);
        case GEAR:
        return tr(M_SETTINGS);
        case GO:
        if (phase == RUNNING) return stopping ? tr(M_STOPPING):tr(M_STOP);
        if (phase == FINISHED) return demo ? tr(M_REPEAT):tr(M_NEW_PREVIEW);
        return demo ? tr(M_START_DEMO):tr(M_ORGANIZE);
        default:
        return T("");
    }
}
static int activeButton(int id) {
    if (id == DETAIL) return plan && phase != SCAN;
    if (id == SPEED || id == EFFECT || id == GEAR) return !closing;
    if (id == GO) {
        if (phase == RUNNING) return!stopping;
        if (phase == FINISHED) return 1;
        return phase == READY && plan && !plan->error && plan->n;
    }
    return phase != SCAN && phase != RUNNING;
}
static void buttonArt(H g, int id, int x, int y, int w, int h, int state) {
    int enabled = activeButton(id), down = state&1, focus = state&16;
    U fill = NAV, edge = NAVEDGE, fg = WHITE, ic = MINT, background = DARK;
    int ico = I_FOLDER, chev = 0;
    switch (id) {
        case CHOOSE:
        fill = MINT;
        edge = MINT;
        fg = DARK;
        ic = GREEN;
        break;
        case METHOD:
        ico = I_GRID;
        chev = 1;
        break;
        case CONFLICT:
        ico = I_COPY;
        chev = 1;
        break;
        case EFFECT:
        ico = I_ANIMATION;
        chev = 1;
        break;
        case SPEED:
        ico = I_SPEED;
        chev = 1;
        break;
        case DEMO:
        ico = demo ? I_RETURN:I_DEMO;
        fill = DARK;
        edge = NAVEDGE;
        fg = PALE;
        ic = PALE;
        break;
        case DETAIL:
        case REFRESH:
        case GEAR:
        background = PAPER;
        fill = WHITE;
        edge = EDGE;
        fg = INK;
        ic = DIM;
        ico = id == DETAIL ? I_LIST:I_REFRESH;
        break;
        case GO:
        background = PAPER;
        fill = phase == RUNNING ? C(0xf4e3d9):TEAL;
        edge = fill;
        fg = phase == RUNNING ? C(0x884428):WHITE;
        ic = fg;
        ico = phase == RUNNING ? I_STOP:(phase == FINISHED ? I_REFRESH:I_PLAY);
        break;
    }
    if (!enabled) {
        if (background == DARK) {
            fill = NAV;
            edge = NAVEDGE;
            fg = C(0x8a9f94);
            ic = fg;
        } else {
            fill = C(0xe4eae2);
            edge = C(0xd9e1d8);
            fg = C(0x68786d);
            ic = fg;
        }
    } else if (down) {
        fill = id == GO ? GREEN:(background == DARK ? NAVEDGE:C(0xe3ede4));
    }
    box(g, x, y, w, h, 0, background);
    card(g, x, y, w, h, 8, edge, fill);
    if (focus && enabled) card(g, x+3, y+3, w-6, h-6, 5, background == DARK ? MINT:MINT, fill);
    if (id == GEAR) {
        gearIcon(g, x+(w>50?12:(w-20)/2), y+(h-20)/2, 20, ic, fill);
        if (w>50) txt(g,caption(id),x+40,y,w-45,h,F_DIM,fg,0);
        return;
    }
    if (id == REFRESH) {
        glyph(g, ico, x+(w-16)/2, y+(h-16)/2, 16, ic);
        return;
    }
    glyph(g, ico, x+(id == GO ? 17:12), y+(h-17)/2, 17, ic);
    S label = caption(id);
    if (id==CONFLICT && conflict && (w<202 || language==SPANISH || language==FRENCH)) label=tr(M_RENAME_BRIEF);
    if (id==METHOD && mode==MONTH && w<202) label=tr(M_MONTH_BRIEF);
    txt(g, label, x+(id == GO ? 43:37), y, w-(chev ? 62:43), h, id == GO ? F_BOLD:F_DIM, fg, 0);
    if (chev) glyph(g, I_CHEV, x+w-23, y+(h-14)/2, 14, PALE);
}
static void tile(H g, int x, int y, int w, int h, int val, S label, int t, U bg, U fg) {
    card(g, x, y, w, h, 10, EDGE, WHITE);
    int by = y+(h-34)/2;
    box(g, x+12, by, 34, 34, 9, bg);
    glyph(g, t, x+20, by+8, 18, fg);
    W b[16] = {0};
    digits(b, 16, (U) val, 0);
    txt(g, b, x+54, y+4, w-60, h-22, F_NUMBER, INK, 0);
    txt(g, label, x+55, y+h-25, w-61, 19, F_SMALL, DIM, 0);
}
static void statLine(Str b, int cap, int n, S word) {
    digits(b, cap, (U) n, 0);
    put(b, cap, T(" "));
    put(b, cap, word);
}
static void classicView(H g, int x, int y, int w, int h) {
    card(g, x, y, w, h, 14, EDGE, WHITE);
    int m = w<545 ? 12:18;
    int inner = w-2*m;
    int a = inner*45/100, b = inner*34/100;
    int left = x+m, right = x+w-m-b;
    txt(g, tr(M_SOURCE_FILES), left, y+10, a, 27, F_BOLD, INK, 0);
    txt(g, tr(M_TARGET_FOLDERS), right, y+10, b, 27, F_BOLD, INK, 0);
    line(g, x+1, y+44, x+w-2, y+44, 1, EDGE);
    int start = y+56, space = h-70;
    if (!plan || !plan->n || plan->error) {
        int cx = x+w/2, cy = y+45+(h-45)/2;
        int ly = cy-29;
        box(g, cx-59, ly-10, 40, 40, 10, C(0xe4efe7));
        document(g, cx-50, ly-3, 25, C(0x6a8f79));
        glyph(g, I_ARROW, cx-9, ly+2, 19, C(0x83a38f));
        folderIcon(g, cx+26, ly-6, 40, C(0xc89a5f), C(0xe3b878));
        S msg = phase == SCAN ? tr(M_READING):(plan && plan->error ? tr(M_UNAVAILABLE):tr(M_NO_FILES));
        txt(g, msg, x+8, cy+13, w-16, 27, F_EMPTY, INK, 1);
        S hint = phase == SCAN ? tr(M_PREPARING):((plan && plan->error) ? tr(M_RETRY):tr(M_ANOTHER_OR_DEMO));
        txt(g, hint, x+8, cy+42, w-16, 22, F_DIM, DIM, 1);
        return;
    }
    int rows = clamp(space/51, 2, 4), gap = 5;
    int rh = clamp((space-(rows-1)*gap)/rows, 32, 60);
    int stride = rh+gap;
    int index = flight >= 0 ? flight:(doneCount<plan->n ? doneCount:plan->n-1);
    if (index<0) index = 0;
    int from = phase == FINISHED ? imax(0, index-rows+1):index;
    int target = plan->e[index].group;
    int gp = (target/rows)*rows;
    int y0 = start;
    int groupRow = target-gp;
    int ay = y0+rh/2, dy = y0+groupRow*stride+rh/2;
    for (int j = 0; j<rows && from+j<plan->n; j++) {
        Entry *e = &plan->e[from+j];
        int r = e->group-gp;
        if (r >= 0 && r<rows) {
            int sy = y0+j*stride+rh/2, ey = y0+r*stride+rh/2;
            curve(g, left+a-1, sy, right+2, ey, j == 0 ? C(0x9acbb1):C(0xe4ece5));
        }
    }
    for (int j = 0; j<rows && from+j<plan->n; j++) {
        Entry *e = &plan->e[from+j];
        int top = y0+j*stride, s = imin(35, rh-10);
        card(g, left, top, a, rh, 8, (flight == from+j) ? C(0x9ac5a8):C(0xe5eae3), e->status == MOVED ? C(0xf6f9f4):C(0xfafbf8));
        int ix = left+8, iy = top+(rh-s)/2;
        box(g, ix, iy, s, s, 7, tints[e->cat]);
        if (flight != from+j) fileArt(g, e, ix+4, iy+4, s-8);
        int tx = ix+s+7;
        txt(g, e->name, tx, top+2, left+a-tx-4, (rh+4)/2, F_FILE, INK, 0);
        S sub = e->status == MOVED && phase == FINISHED ? (demo ? tr(M_SIMULATED):tr(M_MOVED)):tr(catNames[e->cat]);
        txt(g, sub, tx, top+rh/2-1, left+a-tx-4, rh/2, F_SMALL, DIM, 0);
    }
    for (int j = 0; j<rows && gp+j<plan->ng; j++) {
        Group *d = &plan->g[gp+j];
        int top = y0+j*stride;
        int sel = flight >= 0 && j == groupRow;
        card(g, right, top, b, rh, 8, sel ? C(0x69a982):C(0xdbe6db), sel ? C(0xe4f3e7):C(0xf4f8f2));
        int ss = imin(26, rh-8);
        folderIcon(g, right+9, top+(rh-ss)/2, ss, C(0xc79751), C(0xe5ba74));
        int tx = right+ss+16;
        txt(g, d->name, tx, top+2, right+b-tx-5, (rh+4)/2, F_FILE, INK, 0);
        W count[64] = {0};
        if (phase == READY) {
            statLine(count, 64, d->total, fileWord(d->total));
        } else {
            digits(count, 64, d->done, 0);
            put(count, 64, T(" / "));
            statLine(count, 64, d->total, fileWord(d->total));
        }
        txt(g, count, tx, top+rh/2-1, right+b-tx-5, rh/2, F_SMALL, DIM, 0);
    }
    if (flight >= 0) {
        Entry *e = &plan->e[flight];
        int duration = imax(1, (int)flightDuration());
        U elapsed = GetTickCount()-flightTime;
        int t = clamp((int)(elapsed>2000 ? 2000:elapsed)*1000/duration, 0, 1000);
        int eased = (int)((int64_t) t*t*(3000-2*t)/1000000);
        int x1 = left+24, x2 = right+22;
        int cx = x1+(x2-x1)*eased/1000;
        int arc = (int)((int64_t) 4*t*(1000-t)*imin(32, rh/2)/1000000);
        int cy = ay+(dy-ay)*eased/1000-arc;
        if (t>70 && t<940) {
            dot(g, cx-12, cy+2, 4, C(0xd5eadd));
            dot(g, cx-21, cy+4, 2, C(0xe3f1e8));
        }
        int s = 27+(t<500 ? t:1000-t)/120;
        card(g, cx-s/2-3, cy-s/2-3, s+6, s+6, 8, C(0x9abda6), WHITE);
        fileArt(g, e, cx-s/2, cy-s/2, s);
    }
}
#include "motion.c"

static int statsY(void) {
    if (effect == CLASSIC) return shortView() ? 111 : 120;
    return shortView() ? 104 : 111;
}
static int statsHeight(void) {
    if (effect == CLASSIC) return shortView() ? 60 : 70;
    return shortView() ? 51 : 58;
}
static Area stageArea(void) {
    int x = side() + pad();
    int y = statsY() + statsHeight() + (shortView() ? 14 : 21);
    return (Area){x, y, cw - x - pad(), ch - 104 - y};
}
static void drawStage(H g) {
    Area a = stageArea();
    if (a.w < 1 || a.h < 1) return;
    if (effect == CLASSIC) classicView(g, a.x, a.y, a.w, a.h);
    else motionView(g, a, GetTickCount());
}
static void frame(H g, int fakeButtons) {
    int sw = side(), p = pad(), x = sw+p, w = cw-x-p;
    box(g, 0, 0, cw, ch, 0, PAPER);
    box(g, 0, 0, sw, ch, 0, DARK);
    brand(g, 20, 22);
    txt(g, T("InOrdine"), 69, 18, sw-77, 44, F_LOGO, WHITE, 0);
    txt(g, tr(M_WORKING_FOLDER), 20, shortView() ? 61:74, sw-40, 21, F_BOLD, PALE, 0);
    S folder = demo ? tr(M_SAMPLES):(base ? base:T(""));
    Area ca = buttonArea(CHOOSE);
    txt(g, folder, 20, ca.y+ca.h+3, sw-40, shortView() ? 38:44, F_SMALL, PALE, 3);
    int o = optsY();
    line(g, 20, o-12, sw-20, o-12, 1, NAVEDGE);
    txt(g, tr(M_ORGANIZE_BY), 20, o, sw-40, 21, F_BOLD, PALE, 0);
    txt(g, tr(M_IF_EXISTS), 20, o+optionStep(), sw-40, 21, F_BOLD, PALE, 0);
    txt(g, tr(M_ANIMATION), 20, o+2*optionStep(), sw-40, 21, F_BOLD, PALE, 0);
    txt(g, tr(M_SPEED), 20, o+3*optionStep(), sw-40, 21, F_BOLD, PALE, 0);
    int badgeW = demo ? 140:136;
    U badgeBg = demo ? C(0xede8f7):themeWash, badgeFg = demo ? C(0x795d9b):GREEN;
    box(g, x, 29, badgeW, 24, 12, badgeBg);
    dot(g, x+12, 41, 3, badgeFg);
    txt(g, demo ? tr(M_DEMO_MODE):(phase == RUNNING ? tr(M_RUNNING):(phase == FINISHED ? tr(M_RESULTS):tr(M_PREVIEW))), x+20, 29, badgeW-24, 24, F_SMALL, badgeFg, 0);
    S heading = phase == RUNNING ? (stopping ? tr(M_STOPPING_TITLE):tr(M_RUNNING_TITLE)):(phase == FINISHED ? (stopped ? tr(M_STOPPED_TITLE):tr(M_DONE_TITLE)):tr(M_PREVIEW_TITLE));
    txt(g, heading, x-2, 58, w+2, 47, F_HEADING, INK, 0);
    int sy = statsY(), sh = statsHeight(), gap = 9, span = (w-2*gap)/3;
    int n = plan ? plan->n:0, ng = plan ? plan->ng:0;
    tile(g, x, sy, span, sh, n, tr(M_LISTED), I_LIST, themeWash, DIM);
    tile(g, x+span+gap, sy, span, sh, ng, tr(M_DESTINATIONS), I_FOLDER, C(0xf9ecd7), C(0xae7940));
    tile(g, x+2*(span+gap), sy, w-2*(span+gap), sh, successCount, demo ? tr(M_SIMULATED_PL):tr(M_MOVED_PL), I_CHECK, themeWash, GREEN);
    drawStage(g);
    int barY = ch-93;
    box(g, x, barY, w, 4, 2, EDGE);
    if (n && doneCount) box(g, x, barY, (int)((int64_t) w*doneCount/n), 4, 2, GREEN);
    W row[256] = {0}, sub[280] = {0};
    if (phase == SCAN) put(row, 256, tr(M_PREPARING_DOTS));
    else if (plan && plan->error) {
        put(row, 256, tr(M_READ_FAILED));
        put(sub, 280, localizedNote(plan->error));
    }
    else if (phase == FINISHED && runError) {
        put(row, 256, tr(M_SORT_FAILED));
        put(sub, 280, localizedNote(runError));
    }
    else if (phase == FINISHED) {
        statLine(row, 256, successCount, demo ? (successCount == 1 ? tr(M_SIM_ONE):tr(M_SIM_MANY)):(successCount == 1 ? tr(M_MOVE_ONE):tr(M_MOVE_MANY)));
        put(row, 256, T("  ·  "));
        statLine(row, 256, skipCount, skipCount == 1 ? tr(M_SKIP_ONE):tr(M_SKIP_MANY));
        put(row, 256, T("  ·  "));
        statLine(row, 256, errCount, errCount == 1 ? tr(M_ERR_ONE):tr(M_ERR_MANY));
        put(sub, 280, runError ? runError:(stopped ? tr(M_STOP_HINT):tr(M_DONE_HINT)));
    }
    else if (phase == RUNNING) {
        digits(row, 256, doneCount, 0);
        put(row, 256, tr(M_OF));
        statLine(row, 256, n, n == 1 ? tr(M_PROCESSED_ONE):tr(M_PROCESSED_MANY));
        if (doneCount && plan) put(sub, 280, plan->e[doneCount-1].name);
    }
    else {
        put(row, 256, n ? (demo ? tr(M_READY_DEMO):tr(M_READY_SORT)):tr(M_NO_MOVES));
        if (demo) put(sub, 280, tr(M_DEMO_HINT));
        else if (plan) {
            statLine(sub, 280, plan->ignored, plan->ignored == 1 ? tr(M_EXCLUDED_ONE):tr(M_EXCLUDED_MANY));
            put(sub, 280, T("  ·  "));
            statLine(sub, 280, plan->dirs, plan->dirs == 1 ? tr(M_IGNORED_ONE):tr(M_IGNORED_MANY));
        }
    }
    if (phase == FINISHED && historyWarning) {
        sub[0] = 0;
        put(sub, COUNT(sub), tr(M_HISTORY_WRITE_ERROR));
        if (runError) { put(sub, COUNT(sub), T(" · ")); put(sub, COUNT(sub), localizedNote(runError)); }
    }
    int tw = w-180;
    txt(g, row, x, ch-71, tw, 25, F_BOLD, INK, 0);
    txt(g, sub, x, ch-46, tw, 20, F_SMALL, DIM, 0);
    if (fakeButtons) for (int i = CHOOSE; i<END_BUTTON; i++) {
        Area a = buttonArea(i);
        buttonArt(g, i, a.x, a.y, a.w, a.h, 0);
    }
}
