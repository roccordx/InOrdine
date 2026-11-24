enum { PREF_TAB=220, HISTORY_TAB, PREF_LANGUAGE, PREF_EFFECT, FIRST_PALETTE,
       OCEAN_BUTTON, VIOLET_BUTTON, AMBER_BUTTON, CUSTOM_BUTTON,
       HISTORY_REFRESH, PREF_CLOSE, PREF_END };
enum { PREF_PAGE, HISTORY_PAGE };
static H prefButtons[PREF_END-PREF_TAB], sessionsList, filesList, settingsFont;
static int settingsPage, settingsDpi=96, settingsWidth=848, settingsHeight=510, syncingList;
static HistoryView past;
static U customSwatches[16];
static const int runColumns[]={M_DATE,M_FOLDER,M_METHOD,M_MOVED_PL,M_SKIPPED_PL,M_ERRORS,M_OUTCOME};
static const int fileColumns[]={M_ORIGINAL,M_FOLDER,M_FINAL_NAME,M_OUTCOME};
static int prefPx(int n) { return (n*settingsDpi+48)/96; }
static int prefLogical(int n) { return (n*96+settingsDpi/2)/settingsDpi; }
static int languageAllowed(void) { return phase!=RUNNING && phase!=SCAN; }
static H prefButton(int id) { return prefButtons[id-PREF_TAB]; }
static Area prefArea(int id) {
    int w=settingsWidth, h=settingsHeight, inner=w-60, left=(inner-22)/2;
    switch (id) {
        case PREF_TAB: return (Area){24,17,142,36};
        case HISTORY_TAB: return (Area){170,17,132,36};
        case PREF_LANGUAGE: return (Area){30,115,left,38};
        case PREF_EFFECT: return (Area){30,192,left,38};
        case CUSTOM_BUTTON: return (Area){30,353,inner,38};
        case HISTORY_REFRESH: return (Area){w-138,17,114,36};
        case PREF_CLOSE: return (Area){w-135,h-53,111,34};
        default: {
            int i=id-FIRST_PALETTE;
            int x=30+i*(inner+8)/4;
            int next=30+(i+1)*(inner+8)/4;
            return (Area){x,295,next-x-8,43};
        }
    }
}
static S prefCaption(int id) {
    if (id>=FIRST_PALETTE && id<=AMBER_BUTTON) return tr(themeNames[id-FIRST_PALETTE]);
    switch (id) {
        case PREF_TAB: return tr(M_PREFERENCES);
        case HISTORY_TAB: return tr(M_HISTORY);
        case PREF_LANGUAGE: return languageNames[language];
        case PREF_EFFECT: return tr(effectNames[effect]);
        case CUSTOM_BUTTON: return tr(M_COLOR_PICK);
        case HISTORY_REFRESH: return tr(M_REFRESH_HISTORY);
        default: return tr(M_CLOSE);
    }
}
static int prefVisible(int id) {
    if (id==HISTORY_REFRESH) return settingsPage==HISTORY_PAGE;
    if (id>=PREF_LANGUAGE && id<=CUSTOM_BUTTON) return settingsPage==PREF_PAGE;
    return 1;
}
static void prefDrawButton(H g, int id, int x, int y, int w, int h, int state) {
    int palette=id>=FIRST_PALETTE && id<=CUSTOM_BUTTON;
    int active=(id==PREF_TAB && settingsPage==PREF_PAGE) || (id==HISTORY_TAB && settingsPage==HISTORY_PAGE);
    int chosen=palette && id-FIRST_PALETTE==themeIndex;
    int footer=id==PREF_CLOSE, enabled=id!=PREF_LANGUAGE || languageAllowed();
    U background=(id==PREF_TAB || id==HISTORY_TAB || id==HISTORY_REFRESH || footer) ? PAPER:WHITE;
    U fill=footer ? TEAL : (active||chosen) ? themeWash : WHITE;
    U edge=footer ? TEAL : (active||chosen) ? GREEN : EDGE;
    U ink=footer ? WHITE : enabled ? INK : DIM;
    if ((state&1) && enabled) fill=footer?GREEN:themeWash;
    box(g,x,y,w,h,0,background);
    card(g,x,y,w,h,8,edge,fill);
    if (state&16) card(g,x+3,y+3,w-6,h-6,6,footer?MINT:GREEN,fill);
    if (palette) {
        U swatch=id==CUSTOM_BUTTON?C(customColor):presetColors[id-FIRST_PALETTE][BASE_TEAL];
        dot(g,x+19,y+h/2,8,EDGE);
        dot(g,x+19,y+h/2,7,swatch);
        txt(g,prefCaption(id),x+33,y,w-40-(chosen?18:0),h,F_DIM,ink,0);
        if (chosen) glyph(g,I_CHECK,x+w-22,y+(h-13)/2,13,GREEN);
    } else if (id==PREF_LANGUAGE || id==PREF_EFFECT) {
        txt(g,prefCaption(id),x+13,y,w-42,h,F_BODY,ink,0);
        glyph(g,I_CHEV,x+w-25,y+(h-15)/2,15,DIM);
    } else if (footer) txt(g,prefCaption(id),x+6,y,w-12,h,F_BOLD,ink,1);
    else {
        if (id==PREF_TAB) gearIcon(g,x+12,y+(h-17)/2,17,active?GREEN:DIM,fill);
        else glyph(g,id==HISTORY_TAB?I_LIST:I_REFRESH,x+12,y+(h-16)/2,16,active?GREEN:DIM);
        txt(g,prefCaption(id),x+37,y,w-40,h,F_DIM,ink,0);
    }
}
static void prefPreview(H g, int x, int y, int w, int h) {
    card(g,x,y,w,h,10,EDGE,PAPER);
    int rail=clamp(w/4,42,78), left=x+rail+15, right=x+w-14;
    box(g,x+3,y+3,rail-2,h-6,8,DARK);
    box(g,x+13,y+15,18,17,4,MINT);
    box(g,x+13,y+43,rail-27,4,2,PALE);
    box(g,x+13,y+55,rail-33,4,2,NAVEDGE);
    box(g,x+13,y+67,rail-29,4,2,NAVEDGE);
    txt(g,tr(M_PREVIEW_COLORS),left,y+7,right-left,26,F_SMALL,DIM,0);
    int lane=right-left;
    box(g,left,y+39,lane,28,6,themeWash);
    folderIcon(g,left+7,y+44,16,GREEN,TEAL);
    box(g,left+31,y+49,imax(15,lane/3),3,1,GREEN);
    box(g,left,y+h-22,lane,4,2,EDGE);
    box(g,left,y+h-22,lane*2/3,4,2,GREEN);
}
static int historyListTop(void) { return 110; }
static int filesHeadingY(void) { return 123+(settingsHeight-267)*43/100; }
static void prefFrame(H g, int withButtons) {
    int w=settingsWidth, h=settingsHeight;
    box(g,0,0,w,h,0,PAPER);
    line(g,0,68,w,68,1,EDGE);
    if (settingsPage==PREF_PAGE) {
        card(g,16,79,w-32,168,12,EDGE,WHITE);
        Area a=prefArea(PREF_LANGUAGE);
        txt(g,tr(M_LANGUAGE),a.x,89,a.w,23,F_BOLD,INK,0);
        txt(g,tr(M_ANIMATION),a.x,166,a.w,23,F_BOLD,INK,0);
        int previewX=a.x+a.w+22;
        prefPreview(g,previewX,101,w-previewX-31,124);
        card(g,16,258,w-32,145,12,EDGE,WHITE);
        txt(g,tr(M_COLORS),30,265,w-60,25,F_BOLD,INK,0);
        txt(g,languageAllowed()?tr(M_PREF_NOTE):tr(M_LANG_BUSY),25,h-54,w-174,37,F_DIM,DIM,0);
    } else {
        txt(g,tr(M_HISTORY_TITLE),24,72,w-44,33,F_EMPTY,INK,0);
        if (past.runCount) {
            int y=filesHeadingY();
            S root=past.selected>=0?past.runs[past.selected].root:tr(M_HISTORY_HINT);
            txt(g,tr(M_ENTRIES),24,y,imax(116,w/3),27,F_BOLD,INK,0);
            txt(g,root,24+w/3,y,w-w/3-49,27,F_SMALL,DIM,2);
        } else {
            card(g,21,112,w-42,h-192,12,EDGE,WHITE);
            int cy=(112+h-80)/2;
            box(g,w/2-21,cy-48,42,42,10,themeWash);
            glyph(g,I_LIST,w/2-10,cy-37,20,GREEN);
            txt(g,past.readError?tr(M_HISTORY_ERROR):tr(M_HISTORY_EMPTY),35,cy+1,w-70,56,F_BODY,INK,4);
        }
        S note=tr(M_HISTORY_NOTE);
        if (past.readError) note=tr(M_HISTORY_ERROR);
        else if (past.selected>=0 && *past.runs[past.selected].note) note=localizedNote(past.runs[past.selected].note);
        txt(g,note,25,h-54,w-174,37,F_DIM,DIM,0);
    }
    line(g,0,h-68,w,h-68,1,EDGE);
    if (withButtons) for (int i=PREF_TAB;i<PREF_END;i++) if (prefVisible(i)) {
        Area a=prefArea(i);
        prefDrawButton(g,i,a.x,a.y,a.w,a.h,0);
    }
}
static void placePref(H h, Area a) {
    if (h) MoveWindow(h,prefPx(a.x),prefPx(a.y),prefPx(a.x+a.w)-prefPx(a.x),
                      prefPx(a.y+a.h)-prefPx(a.y),1);
}
static void sizeSettings(void) {
    if (!settingsWindow) return;
    Rect r;
    GetClientRect(settingsWindow,&r);
    settingsWidth=prefLogical(r.r);
    settingsHeight=prefLogical(r.b);
    for (int i=PREF_TAB;i<PREF_END;i++) if (prefButton(i)) {
        placePref(prefButton(i),prefArea(i));
        ShowWindow(prefButton(i),prefVisible(i)?5:0);
    }
    int h=settingsHeight,w=settingsWidth,y=filesHeadingY();
    placePref(sessionsList,(Area){24,historyListTop(),w-48,y-historyListTop()-5});
    placePref(filesList,(Area){24,y+31,w-48,h-105-y});
    int show=settingsPage==HISTORY_PAGE && past.runCount;
    if (sessionsList) ShowWindow(sessionsList,show?5:0);
    if (filesList) ShowWindow(filesList,show?5:0);
    InvalidateRect(settingsWindow,0,0);
}
static void listColumns(H list, const int *titles, const int *width, int n, int insert) {
    for (int i=0; list && i<n; i++) {
        LVCol col={0};
        col.mask=4;
        if (insert) { col.mask|=2|8; col.subItem=i; col.cx=prefPx(width[i]); }
        col.text=(Str)tr(titles[i]);
        SendMessageW(list,insert?0x1061:0x1060,(WP)i,(LP)&col);
    }
}
static void tintList(H list) {
    if (!list) return;
    SendMessageW(list,0x1001,0,colorRef(WHITE));
    SendMessageW(list,0x1026,0,colorRef(WHITE));
    SendMessageW(list,0x1024,0,colorRef(INK));
    InvalidateRect(list,0,1);
}
static void syncSettings(void) {
    if (!settingsWindow) return;
    SetWindowTextW(settingsWindow,tr(M_SETTINGS_TITLE));
    for (int i=PREF_TAB;i<PREF_END;i++) if (prefButton(i)) {
        W label[160]={0};
        if (i==PREF_LANGUAGE || i==PREF_EFFECT) {
            put(label,COUNT(label),tr(i==PREF_LANGUAGE?M_LANGUAGE:M_ANIMATION));
            put(label,COUNT(label),T(" · "));
        }
        put(label,COUNT(label),prefCaption(i));
        if (i>=FIRST_PALETTE && i<=CUSTOM_BUTTON && i-FIRST_PALETTE==themeIndex) put(label,COUNT(label),T(" ✓"));
        SetWindowTextW(prefButton(i),label);
        EnableWindow(prefButton(i),i!=PREF_LANGUAGE || languageAllowed());
        InvalidateRect(prefButton(i),0,0);
    }
    if (sessionsList) SetWindowTextW(sessionsList,tr(M_HISTORY_TITLE));
    if (filesList) SetWindowTextW(filesList,tr(M_ENTRIES));
    listColumns(sessionsList,runColumns,0,COUNT(runColumns),0);
    listColumns(filesList,fileColumns,0,COUNT(fileColumns),0);
    InvalidateRect(settingsWindow,0,0);
}
static void historySelection(int index) {
    if (syncingList || index<0 || index>=past.runCount || index==past.selected) return;
    readRunFiles(&past,index);
    SendMessageW(filesList,0x102f,past.fileCount,0);
    if (past.fileCount) SendMessageW(filesList,0x1013,0,0);
    InvalidateRect(filesList,0,1);
    InvalidateRect(settingsWindow,0,0);
}
static void refreshHistory(void) {
    if (!settingsWindow || settingsPage!=HISTORY_PAGE) return;
    int64_t previous=past.selected>=0 && past.selected<past.runCount ? past.runs[past.selected].id : 0;
    syncingList=1;
    readRuns(&past);
    SendMessageW(sessionsList,0x102f,past.runCount,0);
    int selected=0;
    for (int i=0;i<past.runCount;i++) if (past.runs[i].id==previous) { selected=i; break; }
    if (past.runCount) {
        LVItem it={0}; it.stateMask=3; it.state=3;
        SendMessageW(sessionsList,0x102b,(WP)selected,(LP)&it);
        SendMessageW(sessionsList,0x1013,(WP)selected,0);
        readRunFiles(&past,selected);
    }
    SendMessageW(filesList,0x102f,past.fileCount,0);
    syncingList=0;
    sizeSettings();
    InvalidateRect(sessionsList,0,1);
    InvalidateRect(filesList,0,1);
}
static void historyCell(LVDisp *v, int sessions) {
    if (!(v->item.mask&1) || !v->item.text || v->item.max<1) return;
    Str out=v->item.text; int cap=v->item.max, i=v->item.item, colId=v->item.subItem;
    out[0]=0;
    if (sessions && i>=0 && i<past.runCount) {
        PastRun *r=past.runs+i;
        switch (colId) {
            case 0: put(out,cap,r->date); break;
            case 1: put(out,cap,r->root); break;
            case 2: if (r->method>=0 && r->method<COUNT(methodText)) put(out,cap,tr(methodText[r->method])); break;
            case 3: digits(out,cap,r->moved,0); break;
            case 4: digits(out,cap,r->skipped,0); break;
            case 5: digits(out,cap,r->errors,0); break;
            case 6: put(out,cap,pastState(r)); break;
        }
    } else if (!sessions && i>=0 && i<past.fileCount && past.selected>=0) {
        PastFile *f=past.files+i;
        switch (colId) {
            case 0: put(out,cap,f->name); break;
            case 1: put(out,cap,f->folder); break;
            case 2: put(out,cap,*f->actual?f->actual:T("—")); break;
            case 3: put(out,cap,pastStatus(f,past.runs[past.selected].state));
                if (*f->note) { put(out,cap,T(" · ")); put(out,cap,localizedNote(f->note)); }
                break;
        }
    }
}
static H historyList(H owner,S captionText,int id) {
    H list=CreateWindowExW(0x200,T("SysListView32"),captionText,
        WS_CHILD|0x10000|0x1000|1|4|8,0,0,0,0,owner,(H)(intptr_t)id,inst,0);
    if (list) {
        SendMessageW(list,0x2005,1,0);
        SendMessageW(list,0x1036,0,0x10000|0x20|0x4000);
        tintList(list);
    }
    return list;
}
static void historyFont(void) {
    H previous=settingsFont;
    settingsFont=CreateFontW(-prefPx(13),0,0,0,400,0,0,0,1,0,0,5,0,T("Segoe UI"));
    if (sessionsList) SendMessageW(sessionsList,0x30,(WP)settingsFont,1);
    if (filesList) SendMessageW(filesList,0x30,(WP)settingsFont,1);
    if (previous) DeleteObject(previous);
}
static void switchLanguage(int selected) {
    if (selected<0 || selected>=LANG_COUNT || selected==language || !languageAllowed()) return;
    language=selected;
    savePreference(T("Lingua"),(U)selected);
    if (phase==READY && (demo || mode==CATEGORY)) beginScan();
    else {
        if (demo) {
            Str title=join(T("InOrdine"),T(" · "),tr(M_DEMO_MODE));
            SetWindowTextW(mainwin,title); drop(title);
        }
        setButtons();
    }
    translateDetails();
    syncSettings();
}
static void setThemeChoice(int index,U rgb) {
    setPalette(index,rgb);
    savePreference(T("Aspetto"),((U)themeIndex<<24)|customColor);
    tintList(sessionsList); tintList(filesList); tintList(lv);
    setButtons();
}
static int preferenceMenu(int id) {
    H menu=CreatePopupMenu();
    if (!menu) return -1;
    int languages=id==PREF_LANGUAGE, n=languages?LANG_COUNT:EFFECT_COUNT;
    int selected=languages?language:effect;
    for (int i=0;i<n;i++) AppendMenuW(menu,i==selected?8:0,(WP)i+1,
                                      languages?languageNames[i]:tr(effectNames[i]));
    Rect r;
    GetWindowRect(prefButton(id),&r);
    int val=(int)TrackPopupMenu(menu,0x180|2,r.l,r.b+2,0,settingsWindow,0)-1;
    DestroyMenu(menu);
    return val>=0 && val<n?val:-1;
}
static void prefCommand(int id) {
    if (id==2 || id==PREF_CLOSE) { DestroyWindow(settingsWindow); return; }
    if (id<PREF_TAB || id>=PREF_END || !prefVisible(id)) return;
    if (id==PREF_TAB || id==HISTORY_TAB) {
        settingsPage=id==PREF_TAB?PREF_PAGE:HISTORY_PAGE;
        if (settingsPage==HISTORY_PAGE) refreshHistory();
        else sizeSettings();
        syncSettings(); SetFocus(prefButton(id)); return;
    }
    if (id==HISTORY_REFRESH) { refreshHistory(); return; }
    if (id==PREF_LANGUAGE && languageAllowed()) { switchLanguage(preferenceMenu(id)); return; }
    if (id==PREF_EFFECT) {
        int val=preferenceMenu(id);
        if (val>=0 && val!=effect) changeEffect(val);
        return;
    }
    if (id>=FIRST_PALETTE && id<=AMBER_BUTTON) { setThemeChoice(id-FIRST_PALETTE,customColor); return; }
    if (id==CUSTOM_BUTTON) {
        ColorChoice cc={0}; cc.size=sizeof(cc); cc.owner=settingsWindow;
        cc.result=colorRef(customColor); cc.custom=customSwatches; cc.flags=1|2;
        if (ChooseColorW(&cc)) setThemeChoice(CUSTOM_THEME,colorRef(cc.result));
    }
}
static LR settingsProc(H h,U msg,WP w,LP l) {
    switch (msg) {
        case 1: {
            settingsWindow=h; settingsPage=PREF_PAGE; past.selected=-1;
            settingsDpi=(int)GetDpiForWindow(h); if (!settingsDpi) settingsDpi=96;
            for (int i=PREF_TAB;i<PREF_END;i++)
                prefButtons[i-PREF_TAB]=CreateWindowExW(0,T("BUTTON"),prefCaption(i),BTNFLAGS,
                    0,0,0,0,h,(H)(intptr_t)i,inst,0);
            sessionsList=historyList(h,tr(M_HISTORY_TITLE),310);
            filesList=historyList(h,tr(M_ENTRIES),311);
            const int runWidths[]={144,171,133,66,66,62,153};
            const int fileWidths[]={172,135,174,314};
            listColumns(sessionsList,runColumns,runWidths,COUNT(runColumns),1);
            listColumns(filesList,fileColumns,fileWidths,COUNT(fileColumns),1);
            historyFont(); syncSettings(); sizeSettings();
            return 0;
        }
        case WM_SIZE: if (w!=1) sizeSettings(); return 0;
        case 0x24: {
            MinMax *mm=(MinMax*)l; mm->minTrack.x=prefPx(595); mm->minTrack.y=prefPx(532); return 0;
        }
        case 0x2e0: {
            settingsDpi=(int)LO(w);
            Rect *r=(Rect*)l;
            SetWindowPos(h,0,r->l,r->t,r->r-r->l,r->b-r->t,0x14);
            historyFont(); sizeSettings(); return 0;
        }
        case 0x14: return 1;
        case WM_PAINT:
        case 0x2b: {
            int priorDpi=dpi;
            if (priorDpi!=settingsDpi) { clearFonts(); dpi=settingsDpi; }
            if (msg==WM_PAINT) {
                Paint p; H dc=BeginPaint(h,&p); H g=beginG(dc);
                if (g) { prefFrame(g,0); GdipDeleteGraphics(g); }
                EndPaint(h,&p);
            } else {
                DrawItem *d=(DrawItem*)l;
                if (d && d->id>=PREF_TAB && d->id<PREF_END) {
                    H g=beginG(d->dc);
                    if (g) {
                        prefDrawButton(g,(int)d->id,0,0,prefLogical(d->rect.r),
                                       prefLogical(d->rect.b),(int)d->state);
                        GdipDeleteGraphics(g);
                    }
                }
            }
            if (priorDpi!=settingsDpi) { clearFonts(); dpi=priorDpi; }
            return msg==WM_PAINT?0:1;
        }
        case 0x4e: {
            NM *nm=(NM*)l;
            if (!nm) break;
            if (nm->code==(U)-177) {
                historyCell((LVDisp*)l,nm->hwnd==sessionsList); return 0;
            }
            if (nm->hwnd==sessionsList && (nm->code==(U)-101 || nm->code==(U)-115) && !syncingList) {
                int i=(int)SendMessageW(sessionsList,0x100c,(WP)-1,2);
                historySelection(i); return 0;
            }
            break;
        }
        case WM_COMMAND: {
            int id=(int)LO(w);
            if (id==1) {
                H focus=GetFocus();
                for (int i=PREF_TAB;i<PREF_END;i++) if (prefButton(i)==focus) { prefCommand(i); break; }
            } else if (!HI(w)) prefCommand(id);
            return 0;
        }
        case 7: SetFocus(prefButton(settingsPage==PREF_PAGE?PREF_TAB:HISTORY_TAB)); return 0;
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY:
            settingsWindow=sessionsList=filesList=0;
            for (int i=0;i<COUNT(prefButtons);i++) prefButtons[i]=0;
            freeHistoryView(&past);
            if (settingsFont) { DeleteObject(settingsFont); settingsFont=0; }
            if (mainwin && !closing) SetFocus(buttons[GEAR-CHOOSE]);
            return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
static void openSettings(void) {
    if (settingsWindow) { ShowWindow(settingsWindow,9); SetForegroundWindow(settingsWindow); return; }
    Rect r;
    GetWindowRect(mainwin,&r);
    int ww=imin(px(888),r.r-r.l),hh=imin(px(570),r.b-r.t);
    H h=CreateWindowExW(0,T("InOrdineSettings"),tr(M_SETTINGS_TITLE),WS_OVERLAPPEDWINDOW|0x02000000u,
        r.l+(r.r-r.l-ww)/2,r.t+(r.b-r.t-hh)/2,ww,hh,mainwin,0,inst,0);
    if (h) ShowWindow(h,1);
}
