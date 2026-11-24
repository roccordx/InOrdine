#include "core.c"
#include "history.c"
#include "art.c"
#include "screen.c"
#define WM_READY 0x8001
#define WM_RESULT 0x8002
#define WM_DONE 0x8003
#define WM_INIT 0x8004
#define WM_SIZE 5
#define WM_CLOSE 16
#define WM_DESTROY 2
#define WM_PAINT 15
#define WM_COMMAND 0x111
#define WS_OVERLAPPEDWINDOW 0x00cf0000u
#define WS_CHILD 0x40000000u
#define WS_VISIBLE 0x10000000u
#define BTNFLAGS (WS_CHILD|WS_VISIBLE|0x10000u|11u)
typedef struct {
    Plan *p;
    H gate;
    int conflicts;
    int stop, logFault;
} Run;
static Run *job;
static H mainIcon, smallIcon;
static int starting = 1, autoDemo;
static WP gdipToken;
/* Preferences apply to this Windows user across all working directories. */
static H settingsHive(void) {
    return (H)(intptr_t)(int32_t)0x80000001u;
}
static const S settingsPath = T("Software\\InOrdine");
static U readPreference(S key, U fallback) {
    U value=0, bytes=sizeof(value);
    I err=RegGetValueW(settingsHive(),settingsPath,key,0x10,0,&value,&bytes);
    return !err && bytes==sizeof(value) ? value : fallback;
}
static void savePreference(S name, U value) {
    H key = 0;
    I err = RegCreateKeyExW(settingsHive(), settingsPath, 0, 0, 0, 2, 0, &key, 0);
    if (!err) {
        err = RegSetValueExW(key, name, 0, 4, (const B*)&value, sizeof(value));
        RegCloseKey(key);
    }
    if (err) MessageBoxW(mainwin,tr(M_PREF_FAILED),T("InOrdine"),48);
}
static void loadEffect(void) {
    U stored=readPreference(T("Animazione"),BURST);
    effect=stored<EFFECT_COUNT?(int)stored:BURST;
}
static void loadPreferences(void) {
    U lang=readPreference(T("Lingua"),ITALIAN);
    language=lang<LANG_COUNT?(int)lang:ITALIAN;
    U colors=readPreference(T("Aspetto"),0x003c9977);
    setPalette((int)(colors>>24),colors&0xffffff);
    loadEffect();
}
static H settingsWindow;
static void syncSettings(void);
static void tintList(H list);
static void refreshHistory(void);
static void redrawStage(void) {
    Area a = stageArea();
    Rect r = {px(a.x),px(a.y),px(a.x+a.w),px(a.y+a.h)};
    if (mainwin) InvalidateRect(mainwin, &r, 0);
}
static void repaint(void) {
    if (mainwin) InvalidateRect(mainwin, 0, 0);
}
static void setButtons(void) {
    for (int i = CHOOSE; i<END_BUTTON; i++) {
        H b = buttons[i-CHOOSE];
        SetWindowTextW(b, caption(i));
        EnableWindow(b, activeButton(i));
        InvalidateRect(b, 0, 0);
    }
    syncSettings();
    repaint();
}
static void layout(void) {
    Rect r;
    GetClientRect(mainwin, &r);
    cw = logical(r.r);
    ch = logical(r.b);
    for (int i = CHOOSE; i<END_BUTTON; i++) {
        Area a = buttonArea(i);
        MoveWindow(buttons[i-CHOOSE], px(a.x), px(a.y), px(a.x+a.w)-px(a.x), px(a.y+a.h)-px(a.y), 1);
    }
    repaint();
}
static void refreshDetails(void) {
    if (lv) {
        SendMessageW(lv, 0x102f, plan ? plan->n:0, 0);
        InvalidateRect(lv, 0, 1);
    }
}
static S statusText(Entry *e) {
    if (e->status == MOVED) {
        if (demo) return tr(M_SIMULATED);
        return eq(e->name, e->actual) ? tr(M_MOVED):tr(M_RENAMED);
    }
    if (e->status == FAILED) return tr(M_ERROR);
    if (e->status == SKIPPED) return tr(M_SKIPPED);
    if (phase == FINISHED) return tr(M_UNPROCESSED);
    if (e->clash == 2) return tr(M_BLOCKED_FOLDER);
    if (e->clash == 1) return conflict ? tr(M_WILL_RENAME):tr(M_WILL_SKIP);
    return tr(M_WAITING);
}
static void detailCell(LVDisp *v) {
    if (!plan || v->item.item<0 || v->item.item >= plan->n || !(v->item.mask&1)) return;
    Entry *e = &plan->e[v->item.item];
    Str to = v->item.text;
    int cap = v->item.max;
    if (!to || cap<1) return;
    to[0] = 0;
    switch (v->item.subItem) {
        case 0:
        put(to, cap, e->name);
        break;
        case 1:
        put(to, cap, plan->g[e->group].name);
        break;
        case 2:
        put(to, cap, e->actual ? e->actual:(phase==FINISHED || e->status==FAILED || e->status==SKIPPED ? T("—"):e->name));
        break;
        case 3:
        put(to, cap, statusText(e));
        if (e->note) {
            put(to, cap, T(" · "));
            put(to, cap, localizedNote(e->note));
        }
        break;
    }
}
static LR detailProc(H h, U msg, WP w, LP l) {
    switch (msg) {
        case 1:
        {
            details = h;
            lv = CreateWindowExW(0, T("SysListView32"), tr(M_FILE_LIST), WS_CHILD|WS_VISIBLE|0x10000|0x1000|1|4|8, 0, 0, 0, 0, h, 0, inst, 0);
            lvFont = CreateFontW(-px(14), 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, T("Segoe UI"));
            SendMessageW(lv, 0x30, (WP) lvFont, 1);
            tintList(lv);
            SendMessageW(lv, 0x2005, 1, 0);
            SendMessageW(lv, 0x1036, 0, 0x10000|0x20|0x4000);
            const S names[] = {tr(M_ORIGINAL), tr(M_FOLDER), tr(M_FINAL_NAME), tr(M_OUTCOME)};
            const int widths[] = {185, 135, 170, 325};
            for (int j = 0; j<4; j++) {
                LVCol c = {0};
                c.mask = 2|4|8;
                c.cx = px(widths[j]);
                c.text = (Str) names[j];
                c.subItem = j;
                SendMessageW(lv, 0x1061, (WP) j, (LP)&c);
            }
            refreshDetails();
            return 0;
        }
        case WM_SIZE:
        {
            Rect r;
            GetClientRect(h, &r);
            if (lv) MoveWindow(lv, 0, 0, r.r, r.b, 1);
            return 0;
        }
        case 0x4e:
        if (((NM*) l)->code == (U)-177) {
            detailCell((LVDisp*) l);
            return 0;
        }
        break;
        case 7:
        if (lv) SetFocus(lv);
        return 0;
        case WM_CLOSE:
        DestroyWindow(h);
        return 0;
        case WM_DESTROY:
        lv = 0;
        details = 0;
        if (lvFont) {
            DeleteObject(lvFont);
            lvFont = 0;
        }
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}
static void openDetails(void) {
    if (details) {
        SetForegroundWindow(details);
        return;
    }
    Rect r;
    GetWindowRect(mainwin, &r);
    int ww = imin(px(900), r.r-r.l), hh = imin(px(485), r.b-r.t);
    H h = CreateWindowExW(0, T("InOrdineList"), tr(M_DETAIL_TITLE), WS_OVERLAPPEDWINDOW, r.l+(r.r-r.l-ww)/2, r.t+(r.b-r.t-hh)/2, ww, hh, mainwin, 0, inst, 0);
    if (h) ShowWindow(h, 1);
}
static void updateRow(int i) {
    if (lv) SendMessageW(lv, 0x1015, (WP) i, (LP) i);
}
static void translateDetails(void) {
    if (!lv) return;
    SetWindowTextW(details, tr(M_DETAIL_TITLE));
    SetWindowTextW(lv,tr(M_FILE_LIST));
    const int names[]={M_ORIGINAL,M_FOLDER,M_FINAL_NAME,M_OUTCOME};
    for (int i=0;i<COUNT(names);i++) {
        LVCol col={0}; col.mask=4; col.text=(Str)tr(names[i]);
        SendMessageW(lv,0x1060,i,(LP)&col);
    }
    refreshDetails();
}
static const int demoNames[] = {M_DEMO01, M_DEMO02, M_DEMO03, M_DEMO04, M_DEMO05, M_DEMO06, M_DEMO07, M_DEMO08, M_DEMO09, M_DEMO10};
static void demoPlan(Plan *p) {
    for (int i = 0; i<COUNT(demoNames); i++) {
        /* Three example months, independent of the user's files. */U64 v = 133815024000000000ull+(U64)(i%3)*31ull*864000000000ull;
        FT f = {(U) v, (U)(v>>32)};
        add(p, tr(demoNames[i]), f, f, 0, 80u+(U) i);
    }
    sortEntries(p);
}
static U scanThread(H arg) {
    Plan *p = arg;
    scan(p, selfFile);
    PostMessageW(mainwin, WM_READY, 0, (LP) p);
    return 0;
}
static void resetCounts(void) {
    flight = -1;
    historyWarning = 0;
    opening = retainedCount = 0;
    doneCount = successCount = skipCount = errCount = stopped = stopping = 0;
    drop(runError);
    runError = 0;
}
static void beginScan(void) {
    if (phase == RUNNING || (phase == SCAN && !starting)) return;
    starting = 0;
    freePlan(plan);
    plan = 0;
    phase = SCAN;
    resetCounts();
    refreshDetails();
    setButtons();
    Str title = join(T("InOrdine"), T(" · "), demo ? tr(M_DEMO_MODE):base);
    SetWindowTextW(mainwin, title);
    drop(title);
    Plan *p = mem(sizeof(Plan));
    p->demo = demo;
    p->method = mode;
    p->display = dupw(demo ? tr(M_SAMPLES):base);
    p->root = demo ? 0:extended(base);
    if (demo) {
        demoPlan(p);
        PostMessageW(mainwin, WM_READY, 0, (LP) p);
        return;
    }
    H thread = CreateThread(0, 0, scanThread, p, 0, 0);
    if (thread) CloseHandle(thread);
    else {
        p->error = syserr(GetLastError());
        PostMessageW(mainwin, WM_READY, 0, (LP) p);
    }
}
/* After each result the window releases the worker when its animation has finished. */
static U runThread(H arg) {
    Run *r = arg;
    Plan *p = r->p;
    Str err = 0;
    H root = 0;
    Journal journal={0};
    int processed=0, errors=0, deliveryFailed=0;
    if (!p->demo) beginJournal(&journal,p,r->conflicts);
    /* No destination is created while the opening animation is playing. */
    WaitForSingleObject(r->gate, 0xffffffffu);
    if (!__atomic_load_n(&r->stop, __ATOMIC_ACQUIRE) && !p->demo) {
        root = pin(p->root, &err);
        if (root && (p->rootInfo.idLo || p->rootInfo.idHi)) {
            FileInfo curr;
            if (!GetFileInformationByHandle(root, &curr)) err = syserr(GetLastError());
            else if (!sameId(curr, p->rootInfo)) err = dupw(tr(M_FOLDER_CHANGED));
        }
    }
    if (!err) for (int i = 0; i<p->n; i++) {
        if (__atomic_load_n(&r->stop, __ATOMIC_ACQUIRE)) break;
        Result *res = mem(sizeof(Result));
        res->idx = i;
        if (p->demo) {
            res->status = MOVED;
            res->actual = dupw(p->e[i].name);
        } else {
            journalBefore(&journal,i);
            moveOne(p,i,r->conflicts,res);
            journalAfter(&journal,res);
        }
        processed++;
        if (res->status==FAILED) errors++;
        if (!PostMessageW(mainwin, WM_RESULT, 0, (LP) res)) {
            drop(res->actual);
            drop(res->note);
            drop(res);
            deliveryFailed=1;
            break;
        }
        WaitForSingleObject(r->gate, 0xffffffffu);
    }
    if (root) CloseHandle(root);
    if (!p->demo) {
        int state=err || deliveryFailed ? SESSION_ERROR : processed<p->n ? SESSION_STOPPED :
                  errors ? SESSION_ERROR : SESSION_DONE;
        endJournal(&journal,state,err);
        r->logFault=journal.fault;
    }
    PostMessageW(mainwin, WM_DONE, 0, (LP) err);
    return 0;
}
static void releaseOpening(void) {
    if (!opening) return;
    opening = 0;
    KillTimer(mainwin, ANIMATION_TIMER);
    if (job) SetEvent(job->gate);
    redrawStage();
}
static void go(void) {
    if (phase == FINISHED) {
        autoDemo = demo;
        beginScan();
        return;
    }
    if (phase == RUNNING) {
        if (job) {
            stopping = 1;
            __atomic_store_n(&job->stop, 1, __ATOMIC_RELEASE);
            releaseOpening();
            setButtons();
        }
        return;
    }
    if (phase != READY || !plan || plan->error || !plan->n) return;
    Run *r = mem(sizeof(Run));
    r->p = plan;
    r->conflicts = conflict;
    r->gate = CreateEventW(0, 0, 0, 0);
    if (!r->gate) {
        drop(r);
        MessageBoxW(mainwin, tr(M_CANNOT_START), T("InOrdine"), 16);
        return;
    }
    resetCounts();
    job = r;
    phase = RUNNING;
    setButtons();
    H t = CreateThread(0, 0, runThread, r, 0, 0);
    if (t) {
        CloseHandle(t);
        openingTime = sceneEpoch = GetTickCount();
        opening = openingDuration() > 0;
        if (!opening) SetEvent(r->gate);
        else if (!SetTimer(mainwin, ANIMATION_TIMER, 16, 0)) releaseOpening();
    }
    else {
        U ec = GetLastError();
        CloseHandle(r->gate);
        drop(r);
        job = 0;
        phase = READY;
        Str s = syserr(ec);
        MessageBoxW(mainwin, s, T("InOrdine"), 16);
        drop(s);
        setButtons();
    }
}
static void ack(void) {
    KillTimer(mainwin, ANIMATION_TIMER);
    flight = -1;
    if (job) SetEvent(job->gate);
    repaint();
}
static void result(Result *r) {
    Entry *e = &plan->e[r->idx];
    e->actual = r->actual;
    e->note = r->note;
    e->status = r->status;
    doneCount++;
    if (e->status == MOVED) {
        successCount++;
        plan->g[e->group].done++;
    } else {
        if (e->status == SKIPPED) skipCount++;
        else errCount++;
        if (retainedCount < MAX_SCENE_FILES) retainedFiles[retainedCount++] = r->idx;
    }
    updateRow(r->idx);
    if (e->status == MOVED && speed != 2 && !closing) {
        flight = r->idx;
        flightTime = GetTickCount();
        pickupElapsed = flightTime - sceneEpoch;
        if (!SetTimer(mainwin, ANIMATION_TIMER, 16, 0)) ack();
    } else ack();
    drop(r);
    repaint();
}
static const Guid fileDialog = {0xdc1c5a9c, 0xe88a, 0x4dde, {0xa5, 0xa1, 0x60, 0xf8, 0x2a, 0x20, 0xae, 0xf7}};
static const Guid iidFileDialog = {0xd57c7288, 0xd4ad, 0x4768, {0xbe, 0x02, 0x9d, 0x96, 0x95, 0x32, 0xd9, 0x60}};
static const Guid iidShellItem = {0x43826d1e, 0xe718, 0x42ee, {0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe}};
static void release(Com *o) {
    if (o)((U(*)(Com*)) o->v[2])(o);
}
static void setFolder(S path) {
    if (!path || !*path) return;
    Str n = plain(path);
    int l = len(n);
    while (l>3 && n[l-1] == '\\') n[--l] = 0;
    drop(base);
    base = n;
    demo = 0;
    beginScan();
}
static void pickFolder(void) {
    Com *d = 0, *item = 0, *cur = 0;
    Str sel = 0;
    I hr = CoCreateInstance(&fileDialog, 0, 1, &iidFileDialog, &d);
    if (hr<0) {
        MessageBoxW(mainwin, tr(M_PICK_FAILED), T("InOrdine"), 48);
        return;
    }
    U op = 0;
    ((I(*)(Com*, U*)) d->v[10])(d, &op);
    ((I(*)(Com*, U)) d->v[9])(d, op|0x20|0x40|0x800|0x02000000|8);
    ((I(*)(Com*, S)) d->v[17])(d, tr(M_PICK_TITLE));
    if (base && SHCreateItemFromParsingName(base, 0, &iidShellItem, &cur) >= 0) {
        ((I(*)(Com*, Com*)) d->v[12])(d, cur);
        release(cur);
    }
    if (((I(*)(Com*, H)) d->v[3])(d, mainwin) >= 0 && ((I(*)(Com*, Com**)) d->v[20])(d, &item) >= 0) {
        if (((I(*)(Com*, U, Str*)) item->v[5])(item, 0x80058000u, &sel) >= 0) {
            setFolder(sel);
            CoTaskMemFree(sel);
        }
        release(item);
    }
    release(d);
}
/* Only the visual clock changes. The file operation keeps its one acknowledgement. */
static void retimeAnimation(U now, U oldDuration) {
    if (!opening && flight < 0) return;
    U *startTime = opening ? &openingTime : &flightTime;
    U duration = opening ? openingDuration() : flightDuration();
    if (!duration) {
        if (opening) releaseOpening();
        else ack();
    } else if (oldDuration) {
        U elapsed = now - *startTime;
        U adjusted = (U)(((U64)(elapsed < oldDuration ? elapsed : oldDuration) * duration) / oldDuration);
        *startTime = now - adjusted;
    }
}
static void changeSpeed(int selection) {
    U now = GetTickCount();
    U oldDuration = opening ? openingDuration() : flightDuration();
    speed = selection;
    retimeAnimation(now, oldDuration);
    setButtons();
}
static void changeEffect(int selection) {
    U now = GetTickCount();
    U oldDuration = opening ? openingDuration() : flightDuration();
    effect = selection;
    retimeAnimation(now, oldDuration);
    savePreference(T("Animazione"),(U)selection);
    setButtons();
}
static void animationTick(U now) {
    if (opening) {
        if (now - openingTime >= openingDuration()) releaseOpening();
        else redrawStage();
    } else if (flight >= 0) {
        if (now - flightTime >= flightDuration()) ack();
        else redrawStage();
    }
}
static void optionMenu(int id) {
    const int *items;
    int count, current;
    if (id == METHOD) {
        items = methodText;
        count = COUNT(methodText);
        current = mode;
    } else if (id == CONFLICT) {
        items = conflictText;
        count = COUNT(conflictText);
        current = conflict;
    } else if (id == EFFECT) {
        items = effectNames;
        count = EFFECT_COUNT;
        current = effect;
    } else {
        items = speedText;
        count = COUNT(speedText);
        current = speed;
    }
    H menu = CreatePopupMenu();
    for (int i = 0; i<count; i++) AppendMenuW(menu, i == current ? 8:0, i+1, tr(items[i]));
    Rect r;
    GetWindowRect(buttons[id-CHOOSE], &r);
    int val = (int) TrackPopupMenu(menu, 0x180|2, r.l, r.b+2, 0, mainwin, 0)-1;
    DestroyMenu(menu);
    if (val<0 || val >= count || val == current) return;
    if (id == METHOD) {
        mode = val;
        beginScan();
    } else if (id == CONFLICT) {
        conflict = val;
        refreshDetails();
        setButtons();
    } else if (id == EFFECT) changeEffect(val);
    else changeSpeed(val);
}
#include "settings.c"

static void command(int id) {
    if (id<CHOOSE || id >= END_BUTTON || !activeButton(id)) return;
    switch (id) {
        case CHOOSE:
        pickFolder();
        break;
        case METHOD:
        case CONFLICT:
        case EFFECT:
        case SPEED:
        optionMenu(id);
        break;
        case DEMO:
        demo = !demo;
        autoDemo = demo;
        beginScan();
        break;
        case DETAIL:
        if ((plan && plan->error) || runError) MessageBoxW(mainwin, localizedNote(runError ? runError:plan->error), tr(M_DETAIL_DIALOG), 48);
        else openDetails();
        break;
        case REFRESH:
        beginScan();
        break;
        case GEAR:
        openSettings();
        break;
        case GO:
        go();
        break;
    }
}
static H backDC, backBitmap, displacedBitmap;
static int backWidth, backHeight;
static void releaseBuffer(void) {
    if (displacedBitmap && backDC) SelectObject(backDC, displacedBitmap);
    if (backBitmap) DeleteObject(backBitmap);
    if (backDC) DeleteDC(backDC);
    backDC = backBitmap = displacedBitmap = 0;
    backWidth = backHeight = 0;
}
static int resizeBuffer(H dc, int width, int height) {
    if (backDC && width == backWidth && height == backHeight) return 0;
    releaseBuffer();
    backDC = CreateCompatibleDC(dc);
    if (backDC) backBitmap = CreateCompatibleBitmap(dc, width, height);
    if (backBitmap) displacedBitmap = SelectObject(backDC, backBitmap);
    if (!displacedBitmap || displacedBitmap == BAD) {
        displacedBitmap = 0;
        releaseBuffer();
    } else {
        backWidth = width;
        backHeight = height;
    }
    return 1;
}
static void paintWindow(H h) {
    Paint p;
    H dc = BeginPaint(h, &p);
    Rect r;
    GetClientRect(h, &r);
    if (r.r > 0 && r.b > 0) {
        int resized = resizeBuffer(dc, r.r, r.b);
        H g = beginG(backDC ? backDC : dc);
        if (g) {
            Area a = stageArea();
            int stageOnly = !resized && p.rect.l >= px(a.x) && p.rect.t >= px(a.y) &&
                            p.rect.r <= px(a.x+a.w) && p.rect.b <= px(a.y+a.h);
            if (stageOnly) drawStage(g);
            else frame(g, 0);
            GdipDeleteGraphics(g);
        }
        if (backDC) BitBlt(dc,p.rect.l,p.rect.t,p.rect.r-p.rect.l,p.rect.b-p.rect.t,
                           backDC,p.rect.l,p.rect.t,0x00cc0020);
    }
    EndPaint(h, &p);
}
static void showPlan(Plan *p) {
    sceneSeed = mixBits(sceneSeed + GetTickCount() + 0x9e3779b9u + hashw(p->display));
    plan = p;
    phase = READY;
    refreshDetails();
    setButtons();
    if (autoDemo) {
        autoDemo = 0;
        go();
    }
}
static void finishRun(Str error) {
    if (job) {
        historyWarning=job->logFault;
        CloseHandle(job->gate);
        drop(job);
        job = 0;
    }
    runError = error;
    stopped = (stopping && plan && doneCount<plan->n) || runError;
    phase = FINISHED;
    flight = -1;
    stopping = 0;
    refreshDetails();
    setButtons();
    if (!closing && !demo) refreshHistory();
    if (closing) DestroyWindow(mainwin);
}
static LR windowProc(H h, U m, WP w, LP l) {
    switch (m) {
        case 1:
        mainwin = h;
        dpi = (int) GetDpiForWindow(h);
        if (!dpi) dpi = 96;
        for (int i = CHOOSE; i<END_BUTTON; i++) buttons[i-CHOOSE] = CreateWindowExW(0, T("BUTTON"), caption(i), BTNFLAGS, 0, 0, 0, 0, h, (H)(intptr_t) i, inst, 0);
        DragAcceptFiles(h, 1);
        layout();
        PostMessageW(h, WM_INIT, 0, 0);
        return 0;
        case WM_INIT:
        beginScan();
        return 0;
        case WM_SIZE:
        if (w != 1 && mainwin) layout();
        return 0;
        case 0x24:
        {
            MinMax *mm = (MinMax*) l;
            mm->minTrack.x = px(795);
            mm->minTrack.y = px(545);
            return 0;
        }
        case 0x2e0:
        {
            dpi = (int) LO(w);
            clearFonts();
            Rect *r = (Rect*) l;
            SetWindowPos(h, 0, r->l, r->t, r->r-r->l, r->b-r->t, 0x14);
            layout();
            return 0;
        }
        case 0x14:
        return 1;
        case WM_PAINT:
        paintWindow(h);
        return 0;
        case 0x2b:
        {
            DrawItem *d = (DrawItem*) l;
            if (d && d->id >= CHOOSE && d->id<END_BUTTON) {
                H g = beginG(d->dc);
                if (g) {
                    buttonArt(g, (int) d->id, 0, 0, logical(d->rect.r), logical(d->rect.b), (int) d->state);
                    GdipDeleteGraphics(g);
                }
                return 1;
            }
            break;
        }
        case WM_COMMAND:
        {
            int id = LO(w);
            if (id == 1) {
                H f = GetFocus();
                for (int i = CHOOSE; i<END_BUTTON; i++) if (buttons[i-CHOOSE] == f) {
                    command(i);
                    return 0;
                }
            } else if (id >= CHOOSE && id<END_BUTTON && HI(w) == 0) command(id);
            return 0;
        }
        case 0x233:
        {
            H files = (H) w;
            if (phase != SCAN && phase != RUNNING && DragQueryFileW(files, 0xffffffffu, 0, 0) == 1) {
                U n = DragQueryFileW(files, 0, 0, 0);
                Str s = mem((n+1)*2);
                DragQueryFileW(files, 0, s, n+1);
                Str e = extended(s);
                U a = GetFileAttributesW(e);
                drop(e);
                if (a != 0xffffffffu && (a&DIR_ATTR)) setFolder(s);
                else MessageBoxW(h, tr(M_DROP_HINT), T("InOrdine"), 64);
                drop(s);
            }
            DragFinish(files);
            return 0;
        }
        case WM_READY:
        showPlan((Plan*) l);
        return 0;
        case WM_RESULT:
        result((Result*) l);
        return 0;
        case WM_DONE:
        finishRun((Str) l);
        return 0;
        case 0x113:
        if (w == ANIMATION_TIMER) animationTick(GetTickCount());
        return 0;
        case WM_CLOSE:
        if (phase == RUNNING) {
            closing = stopping = 1;
            __atomic_store_n(&job->stop, 1, __ATOMIC_RELEASE);
            if (opening) releaseOpening();
            else if (flight >= 0) ack();
            setButtons();
            return 0;
        }
        DestroyWindow(h);
        return 0;
        case WM_DESTROY:
        releaseBuffer();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
static int registerWindow(S name, LR(*cb)(H, U, WP, LP)) {
    WClass wc = {0};
    wc.size = sizeof(wc);
    wc.style = 3;
    wc.proc = cb;
    wc.instance = inst;
    wc.icon = mainIcon;
    wc.iconSmall = smallIcon;
    wc.cursor = LoadCursorW(0, RES(32512));
    wc.className = name;
    return RegisterClassExW(&wc) != 0;
}
U GetFullPathNameW(S, U, Str, Str*);
static void initialPath(void) {
    U cap = 512, n;
    Str module = 0;
    for (;;) {
        module = grow(module, (size_t) cap*2);
        n = GetModuleFileNameW(0, module, cap);
        if (!n) {
            drop(module);
            module = dupw(T("C:\\InOrdine.exe"));
            break;
        }
        if (n<cap) break;
        cap *= 2;
    }
    selfFile = extended(module);
    H exe = CreateFileW(selfFile, ATTR_READ, SHARE_ALL, 0, OPEN_EXISTING, OPEN_LINK, 0);
    if (exe != BAD) {
        knownSelf = GetFileInformationByHandle(exe, &selfInfo);
        CloseHandle(exe);
    }
    Str display = plain(module);
    drop(module);
    int k = (int)(baseName(display)-display);
    if (k>3 && display[k-1] == '\\') k--;
    base = part(display, k);
    drop(display);
    I argc = 0;
    W **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 2) {
        U count = GetFullPathNameW(argv[1], 0, 0, 0);
        if (count && count<32768) {
            Str given = mem((size_t)(count+1)*2);
            if (GetFullPathNameW(argv[1], count+1, given, 0)) {
                drop(base);
                base = plain(given);
            }
            drop(given);
        }
    }
    if (argv) LocalFree(argv);
}
void start(void) {
    hp = GetProcessHeap();
    inst = GetModuleHandleW(0);
    dpi = (int) GetDpiForSystem();
    if (!dpi) dpi = 96;
    GPStart gp = {1, 0, 0, 0};
    if (GdiplusStartup(&gdipToken, &gp, 0)) {
        MessageBoxW(0, tr(M_GRAPHICS_FAILED), T("InOrdine"), 16);
        ExitProcess(1);
    }
    I co = CoInitializeEx(0, 2|4);
    InitCC cc = {sizeof(cc), 1|0x4000};
    InitCommonControlsEx(&cc);
    initArt();
    loadPreferences();
    initHistory();
    initialPath();
    mainIcon = LoadIconW(inst, RES(1));
    smallIcon = LoadImageW(inst, RES(1), 1, GetSystemMetrics(49), GetSystemMetrics(50), 0x8000);
    if (!registerWindow(T("InOrdineMain"), windowProc) || !registerWindow(T("InOrdineList"), detailProc) || !registerWindow(T("InOrdineSettings"), settingsProc)) ExitProcess(2);
    Rect work = {0, 0, GetSystemMetrics(0), GetSystemMetrics(1)};
    SystemParametersInfoW(48, 0, &work, 0);
    Rect r = {0, 0, px(1080), px(670)};
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, 0, 0);
    int ww = imin(r.r-r.l, work.r-work.l), hh = imin(r.b-r.t, work.b-work.t);
    H h = CreateWindowExW(0, T("InOrdineMain"), T("InOrdine"), WS_OVERLAPPEDWINDOW|0x02000000u, work.l+(work.r-work.l-ww)/2, work.t+(work.b-work.t-hh)/2, ww, hh, 0, 0, inst, 0);
    if (!h) {
        MessageBoxW(0, tr(M_WINDOW_FAILED), T("InOrdine"), 16);
        ExitProcess(2);
    }
    ShowWindow(h, 1);
    UpdateWindow(h);
    Msg msg;
    while (GetMessageW(&msg, 0, 0, 0)>0) {
        if (settingsWindow && IsDialogMessageW(settingsWindow, &msg)) continue;
        if (details && IsDialogMessageW(details, &msg)) continue;
        if (!IsDialogMessageW(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    clearFonts();
    for (int i = 0; i<3; i++) if (family[i]) GdipDeleteFontFamily(family[i]);
    for (int i = 0; i<6; i++) if (fmt[i]) GdipDeleteStringFormat(fmt[i]);
    for (int i = 0; i<iconCount; i++) if (iconCache[i].image) GdipDisposeImage(iconCache[i].image);
    GdipDeleteBrush(brush);
    GdiplusShutdown(gdipToken);
    if (co >= 0) CoUninitialize();
    ExitProcess(0);
}
