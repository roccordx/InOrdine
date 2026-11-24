/* The canvas only reads the preview and confirmed results. It never changes the file plan. */
typedef struct { float x, y; } Position;

typedef struct {
    Area panel, field;
    Position source;
    int rows, cols, slots, cells[28];
    int dockTop, dockHeight, dockCount, dockStart, dockWidth, dockLeft, gap;
    int tileSize, compact;
} SceneLayout;

static const U fileColors[] = {
    C(0x88d9ef), C(0xffb294), C(0xbca9ff), C(0xf3a1d1),
    C(0xf5cf8a), C(0x9eace9), C(0x89dfb9), C(0xb9d3cc)
};

static U mixBits(U value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

static float withinUnit(float value) {
    return value < 0.f ? 0.f : (value > 1.f ? 1.f : value);
}

static float smooth(float t) {
    t = withinUnit(t);
    return t * t * (3.f - 2.f * t);
}

static float springOut(float t) {
    float v = withinUnit(t) - 1.f;
    return 1.f + v*v*(2.15f*v + 1.15f);
}

/* A turn has 4096 units. Interpolating a quarter wave avoids a runtime math dependency. */
static float sineTurn(int angle) {
    static const W quarter[] = {
        0,25,50,75,100,125,150,175,200,224,249,273,297,321,345,369,
        392,415,438,460,483,505,526,548,569,590,610,630,650,669,688,706,
        724,742,759,775,792,807,822,837,851,865,878,891,903,915,926,937,
        946,956,964,973,980,987,994,999,1004,1009,1013,1016,1019,1021,1023,1024,1024
    };
    int a = (int)((U)angle & 4095u);
    int q = a / 1024, offset = a % 1024;
    if (q & 1) offset = 1024 - offset;
    int i = offset / 16, rem = offset % 16;
    float value = quarter[i];
    if (rem) value += (quarter[i+1] - quarter[i]) * (rem / 16.f);
    return (q >= 2 ? -value : value) / 1024.f;
}

static Position between(Position a, Position b, float t) {
    return (Position){a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t};
}

static Position cubic(Position a, Position b, Position c, Position d, float t) {
    Position ab = between(a,b,t), bc = between(b,c,t), cd = between(c,d,t);
    return between(between(ab,bc,t), between(bc,cd,t), t);
}

static U blend(U a, U b, int weight) {
    int t = clamp(weight, 0, 255), w = 255 - t;
    U red = (((a>>16)&255)*w + ((b>>16)&255)*t) / 255;
    U green = (((a>>8)&255)*w + ((b>>8)&255)*t) / 255;
    U blue = ((a&255)*w + (b&255)*t) / 255;
    return 0xff000000u | (red<<16) | (green<<8) | blue;
}

static U alpha(U c, int opacity) {
    return (c & 0xffffffu) | ((U)clamp(opacity,0,255) << 24);
}

static U sceneAccent(void) {
    if (themeIndex) return effect == BURST ? MINT : blend(MINT, C(0xd8b7ff), 68);
    return effect == BURST ? C(0x81efcf) : C(0xc2adff);
}

static U dockColor(int group) {
    if (plan->method == CATEGORY) return fileColors[plan->g[group].category];
    return fileColors[(U)group % COUNT(fileColors)];
}

static void ellipseFill(H g, float x, float y, float rx, float ry, U color) {
    if (rx < 1.f || ry < 1.f) return;
    col(color);
    GdipFillEllipseI(g, brush, px((int)(x-rx)), px((int)(y-ry)),
                     px((int)(rx*2.f)), px((int)(ry*2.f)));
}

static void ellipseLine(H g, Position p, float rx, float ry, int width, U color) {
    if (rx < 1.f || ry < 1.f) return;
    H pen = 0;
    if (GdipCreatePen1(color, (float)px(width), 2, &pen)) return;
    GdipDrawEllipseI(g, pen, px((int)(p.x-rx)), px((int)(p.y-ry)),
                    px((int)(2.f*rx)), px((int)(2.f*ry)));
    GdipDeletePen(pen);
}

static void softLight(H g, Position center, int rx, int ry, U hue, int opacity) {
    if (rx < 2 || ry < 2) return;
    int x=px((int)center.x-rx), y=px((int)center.y-ry);
    H path=0, gradient=0;
    if (GdipCreatePath(0,&path)) return;
    GdipAddPathEllipseI(path,x,y,px(2*rx),px(2*ry));
    if (!GdipCreatePathGradientFromPath(path,&gradient)) {
        U edge=alpha(hue,0);
        int count=1;
        GdipSetPathGradientCenterColor(gradient,alpha(hue,opacity));
        GdipSetPathGradientSurroundColorsWithCount(gradient,&edge,&count);
        GdipFillPath(g,gradient,path);
        GdipDeleteBrush(gradient);
    }
    GdipDeletePath(path);
}

static void colorPolygon(H g, const Point *points, int n, U top, U bottom) {
    Point pixels[16];
    int minX=points[0].x,maxX=minX,minY=points[0].y,maxY=minY;
    for (int i=0;i<n;i++) {
        minX=imin(minX,points[i].x); maxX=imax(maxX,points[i].x);
        minY=imin(minY,points[i].y); maxY=imax(maxY,points[i].y);
        pixels[i]=(Point){px(points[i].x),px(points[i].y)};
    }
    Point a={px(minX),px(minY)}, b={px(maxX),px(maxY)};
    H gradient=0;
    if (GdipCreateLineBrushI(&a,&b,top,bottom,0,&gradient)) {
        polygon(g,points,n,top);
        return;
    }
    GdipFillPolygonI(g,gradient,pixels,n,0);
    GdipDeleteBrush(gradient);
}

static void stageBackdrop(H g, Area a) {
    U near = effect == BURST ? C(0x173b40) : C(0x302e4b);
    U far = effect == BURST ? C(0x152333) : C(0x181f35);
    if (themeIndex) {
        near = blend(NAV,sceneAccent(),effect==BURST?36:22);
        far = blend(DARK,C(0x111829),90);
    }
    Point first = {px(a.x),px(a.y)}, last = {px(a.x+a.w),px(a.y+a.h/2)};
    H gradient = 0;
    if (GdipCreateLineBrushI(&first, &last, near, far, 0, &gradient)) {
        box(g,a.x,a.y,a.w,a.h,15,near);
        return;
    }
    H path = roundedPath(px(a.x),px(a.y),px(a.x+a.w)-px(a.x),px(a.y+a.h)-px(a.y),px(30));
    GdipFillPath(g, gradient, path);
    GdipDeletePath(path);
    GdipDeleteBrush(gradient);
}

static int focusFile(void) {
    if (flight >= 0) return flight;
    if (!plan || !plan->n) return -1;
    if (phase == FINISHED && doneCount) return doneCount - 1;
    return imin(doneCount, plan->n - 1);
}

static SceneLayout sceneLayout(Area a) {
    SceneLayout layout = {0};
    layout.panel = a;
    layout.compact = a.h < 265;
    layout.gap = a.w < 560 ? 5 : 8;
    int margin = a.w < 560 ? 12 : 17;
    layout.dockHeight = layout.compact ? 43 : 51;
    layout.dockTop = a.y + a.h - layout.dockHeight - (layout.compact ? 8 : 12);
    int top = a.y + (layout.compact ? 29 : 38);
    layout.field = (Area){a.x + margin, top, a.w - margin*2,
                          imax(36, layout.dockTop - top - (layout.compact ? 22 : 29))};
    Area f = layout.field;
    layout.source = (Position){f.x + f.w*.5f, f.y + f.h*.49f};
    layout.cols = f.w >= 735 ? 7 : (f.w >= 420 ? 5 : 3);
    layout.rows = clamp(f.h / 61, 2, 4);
    int middleCol = layout.cols / 2;
    int count = 0;
    for (int row = 0; row < layout.rows; ++row) {
        for (int col = 0; col < layout.cols; ++col) {
            if (col == middleCol && row >= (layout.rows-1)/2 && row <= layout.rows/2) continue;
            layout.cells[count++] = row * layout.cols + col;
        }
    }
    U state = mixBits(sceneSeed ^ (U)count);
    for (int i = count - 1; i > 0; --i) {
        state = mixBits(state + 0x9e3779b9u);
        int j = (int)(state % (U)(i+1));
        int cell = layout.cells[i];
        layout.cells[i] = layout.cells[j];
        layout.cells[j] = cell;
    }
    layout.slots = imin(plan ? plan->n : 0, imin(count, MAX_SCENE_FILES));
    if (effect == ORBIT) layout.slots = imin(layout.slots, f.h < 170 ? 8 : 14);
    layout.tileSize = clamp(imin(f.h/layout.rows - 24, f.w/layout.cols - 25), 22, layout.compact ? 34 : 44);

    if (plan && plan->ng) {
        int perPage = clamp(f.w / 125, 2, 6);
        int idx = focusFile();
        int group = idx < 0 ? 0 : plan->e[idx].group;
        layout.dockStart = (group / perPage) * perPage;
        layout.dockCount = imin(perPage, plan->ng - layout.dockStart);
        /* Reserve every page's slots so folders do not change size on the last page. */
        int columns = imin(perPage, plan->ng);
        layout.dockWidth = imin(188, (f.w - (columns-1)*layout.gap) / columns);
        layout.dockLeft = f.x + (f.w - columns*layout.dockWidth - (columns-1)*layout.gap)/2;
    }
    return layout;
}

static Position dockMouth(const SceneLayout *s, int group) {
    int x = s->dockLeft + (group-s->dockStart)*(s->dockWidth+s->gap);
    return (Position){x + (s->dockWidth < 115 ? 21 : 24),
                      s->dockTop + s->dockHeight/2.f - 2.f};
}

static Position scatterSpot(const SceneLayout *s, int index, int slot, U ms) {
    int cell = s->cells[slot], column = cell % s->cols, row = cell / s->cols;
    U seed = mixBits(sceneSeed ^ ((U)index + 1u)*0xa511e9b3u);
    float cellW = s->field.w / (float)s->cols, cellH = s->field.h / (float)s->rows;
    float jitterX = (((int)(seed & 255)-127) / 127.f) * cellW * .24f;
    float jitterY = (((int)((seed>>8) & 255)-127) / 127.f) * (cellH-s->tileSize-16.f) * .34f;
    int phaseAngle = (int)((ms % 5200u)*4096u/5200u) + (int)(seed & 4095);
    return (Position){s->field.x + (column+.5f)*cellW + jitterX + 2.f*sineTurn(phaseAngle),
                      s->field.y + (row+.5f)*cellH - 5.f + jitterY + 2.f*sineTurn(phaseAngle+1150)};
}

static Position orbitSpot(const SceneLayout *s, int slot, U ms) {
    int lanes = s->slots > 3 ? 2 : 1, lane = slot % lanes, rank = slot / lanes;
    int count = (s->slots - lane + lanes-1) / lanes;
    U period = lane ? 12700u : 17300u;
    int turn = (int)((ms % period)*4096u/period);
    int angle = (int)(sceneSeed & 4095u) + (lane ? 1250 - turn : turn) + rank*4096/count;
    float rx = s->field.w*(lane ? .265f : .40f);
    float ry = s->field.h*(lane ? .245f : .342f);
    return (Position){s->source.x + rx*sineTurn(angle+1024), s->source.y + ry*sineTurn(angle)};
}

static Position restingSpot(const SceneLayout *s, int index, int slot, U ms) {
    return effect == BURST ? scatterSpot(s,index,slot,ms) : orbitSpot(s,slot,ms);
}

static Position openingSpot(const SceneLayout *s, Position rest, int index, float t) {
    float delay = (mixBits(sceneSeed ^ (U)index) & 255u) / 255.f * .10f;
    if (t < .19f) return between(rest, s->source, smooth(t/.19f));
    float u = withinUnit((t-.19f-delay) / (.81f-delay));
    if (effect == BURST) return between(s->source, rest, springOut(u));
    Position bend = {s->source.x - (rest.y-s->source.y)*.65f,
                     s->source.y + (rest.x-s->source.x)*.12f};
    return cubic(s->source,bend,rest,rest,smooth(u));
}

static Position flightSpot(const SceneLayout *s, Position from, Position to, float t) {
    float u = smooth(t);
    if (effect == ORBIT) {
        float dy = from.y - s->source.y;
        Position tangent = {from.x + (dy >= 0 ? -1.f : 1.f)*s->field.w*.11f,
                            from.y + (from.x-s->source.x)*.12f};
        tangent.x = (float)clamp((int)tangent.x, s->field.x+12, s->field.x+s->field.w-12);
        tangent.y = (float)clamp((int)tangent.y, s->field.y+8, s->field.y+s->field.h-8);
        Position above = {to.x, to.y - s->field.h*.40f};
        return cubic(from,tangent,above,to,u);
    }
    float bend = (from.x < s->source.x ? -1.f : 1.f) * (s->compact ? 11.f : 21.f);
    if (u < .43f) {
        Position first = {from.x + bend, from.y - 13.f};
        Position guide = {s->source.x + bend, s->source.y - 16.f};
        return cubic(from,first,guide,s->source,u/.43f);
    }
    Position guide = {s->source.x - bend, s->source.y + 23.f};
    Position above = {to.x, to.y - s->field.h*.31f};
    return cubic(s->source,guide,above,to,(u-.43f)/.57f);
}

static void flatFile(H g, Entry *e, Position p, int size, int opacity) {
    if (size < 6 || opacity < 5) return;
    U accent = fileColors[e->cat], ink = blend(C(0x192c38),accent,52);
    int h = size, w = imax(6,size*77/100), x = (int)p.x - w/2, y = (int)p.y-h/2;
    int cut = imax(3,w/3), bevel = imax(1,w/12);
    Point paper[] = {{x+bevel,y}, {x+w-cut,y}, {x+w,y+cut}, {x+w,y+h-bevel},
                     {x+w-bevel,y+h}, {x+bevel,y+h}, {x,y+h-bevel}, {x,y+bevel}};
    box(g,x+2,y+4,w,h,bevel+1,alpha(C(0x061320),opacity/2));
    colorPolygon(g,paper,COUNT(paper),alpha(blend(accent,WHITE,224),opacity),
                 alpha(blend(accent,WHITE,79),opacity));
    Point fold[] = {{x+w-cut,y}, {x+w,y+cut}, {x+w-cut,y+cut}};
    polygon(g,fold,COUNT(fold),alpha(blend(accent,ink,55),opacity));
    if (size < 21) return;
    int gx=x+w/4, gy=y+h*31/100, gw=imax(7,w*49/100), gh=imax(4,h/5);
    U mark = alpha(blend(accent,ink,120),opacity);
    if (e->cat == IMAGE) {
        Point mountains[] = {{gx,gy+gh}, {gx+gw/3,gy+1}, {gx+gw*53/100,gy+gh/2},
                             {gx+gw*72/100,gy}, {gx+gw,gy+gh}};
        polygon(g,mountains,COUNT(mountains),mark);
    } else if (e->cat == AUDIO || e->cat == ARCHIVE) {
        for (int i=0;i<4;i++) {
            int height = e->cat == AUDIO ? gh*(i==1?4:(i==2?3:2))/4 : 2;
            int dx = e->cat == AUDIO ? gx+i*gw/4 : gx+(i%2)*3;
            int dy = e->cat == AUDIO ? gy+gh-height : gy+i*2;
            box(g,dx,dy,imax(2,gw/5),height,1,mark);
        }
    } else if (e->cat == VIDEO) {
        Point play[]={{gx+2,gy},{gx+gw-1,gy+gh/2},{gx+2,gy+gh}};
        polygon(g,play,COUNT(play),mark);
    } else {
        box(g,gx,gy,gw,2,1,mark);
        box(g,gx,gy+4,gw*2/3,2,1,mark);
    }
    if (size < 28) return;
    S ext=suffix(e->name); W label[6]={0};
    for (int i=0;i<5 && ext[i];i++) label[i]=(ext[i]>='a' && ext[i]<='z') ? ext[i]-32 : ext[i];
    int tagW=imax(24,imin(w+7,len(label)*7+7));
    int tagH=clamp(h/3,12,16);
    box(g,x-2,y+h-tagH-2,tagW,tagH,3,alpha(accent,opacity));
    txt(g,label,x-2,y+h-tagH-3,tagW,tagH+2,F_TINY,alpha(ink,opacity),1);
}

static void fileTile(H g, Entry *e, Position p, int size, int opacity, float angle) {
    U saved=0;
    if ((angle > -.15f && angle < .15f) || GdipSaveGraphics(g,&saved)) {
        flatFile(g,e,p,size,opacity);
        return;
    }
    GdipTranslateWorldTransform(g,(float)px((int)p.x),(float)px((int)p.y),0);
    GdipRotateWorldTransform(g,angle,0);
    flatFile(g,e,(Position){0,0},size,opacity);
    GdipRestoreGraphics(g,saved);
}

/* The back, contents and lip are separate so an arriving file can disappear into the opening. */
static void folderShape(H g, Position mouth, int width, U hue, float open, int onlyLip) {
    int w=width, h=w*65/100, x=(int)mouth.x-w/2, y=(int)mouth.y-h/2;
    int toe=imax(1,w/16), lift=(int)(open*h*.14f);
    int shoulder=y+h/4, lip=y+h*41/100+lift;
    U rim=blend(hue,WHITE,63), inner=blend(C(0x182c41),hue,88);
    if (!onlyLip) {
        Point back[]={{x+1,y+toe},{x+toe,y},{x+w*38/100,y},{x+w/2,shoulder},
                      {x+w-toe,shoulder},{x+w,shoulder+toe},{x+w,y+h-toe},
                      {x+w-toe,y+h},{x+toe,y+h},{x,y+h-toe}};
        colorPolygon(g,back,COUNT(back),blend(inner,hue,75),inner);
        line(g,x+toe,y+1,x+w*38/100,y+1,1,rim);
        int pageTop=shoulder-(int)(h*.12f*open);
        box(g,x+w/9,pageTop,w*74/100,h/3,2,blend(hue,WHITE,150));
        box(g,x+w/6,pageTop+3,w*70/100,h/3,2,blend(hue,WHITE,69));
    }
    Point front[]={{x-1,lip+toe},{x+toe,lip},{x+w-toe,lip},{x+w+1,lip+toe},
                   {x+w-toe,y+h},{x+toe,y+h}};
    colorPolygon(g,front,COUNT(front),blend(hue,WHITE,54),blend(hue,inner,64));
    line(g,x+toe,lip+1,x+w-toe,lip+1,1,rim);
    if (width >= 49) {
        box(g,x+w*67/100,lip+(y+h-lip)/2,w/6,2,1,blend(hue,inner,115));
        box(g,x+w*67/100,lip+(y+h-lip)/2+4,w/10,1,0,blend(hue,inner,94));
    }
}

static void drawEnvironment(H g, const SceneLayout *s, float intro, float trip) {
    Position c=s->source;
    U accent=sceneAccent();
    for (int i=0;i<32;i++) {
        U key=mixBits(sceneSeed + 0x9e3779b9u*(U)(i+1));
        int xx=s->field.x+5+(int)((key&65535u)%(U)imax(1,s->field.w-10));
        int yy=s->field.y+3+(int)(((key>>16)&65535u)%(U)imax(1,s->field.h-6));
        dot(g,xx,yy,1,alpha(accent,i%4==0?92:40));
    }
    int glow=clamp(s->field.h/2,39,105);
    softLight(g,c,glow+12,(glow+12)*3/4,accent,63);
    Position secondary={s->field.x+s->field.w*.74f,s->field.y+s->field.h*.42f};
    softLight(g,secondary,imin(130,s->field.w/5),imin(76,s->field.h/3),
              effect==BURST ? C(0x3896a5) : C(0x7275bd),23);
    if (effect == ORBIT) {
        ellipseLine(g,c,s->field.w*.40f,s->field.h*.342f,1,alpha(accent,85));
        if (s->slots > 3) ellipseLine(g,c,s->field.w*.265f,s->field.h*.245f,1,alpha(accent,57));
    } else {
        ellipseLine(g,c,glow*.59f,glow*.47f,1,alpha(accent,37));
        ellipseLine(g,c,glow*.76f,glow*.60f,1,alpha(accent,21));
    }
    if (opening && intro>.16f && intro<.97f) {
        float t=withinUnit((intro-.16f)/.81f), progress=1.f-(1.f-t)*(1.f-t);
        int opacity=(int)(154*(1.f-t));
        ellipseLine(g,c,(.08f+progress*.35f)*s->field.w,(.09f+progress*.39f)*s->field.h,1,alpha(accent,opacity));
        if (effect == BURST) {
            for (int i=0;i<25;i++) {
                U key=mixBits(sceneSeed + (U)i*747796405u);
                int angle=(int)(key & 4095);
                float depth=.60f+(float)((key>>12)&255)/640.f;
                float vx=sineTurn(angle+1024)*s->field.w*.46f*depth;
                float vy=sineTurn(angle)*s->field.h*.47f*depth;
                float tail=progress*(.83f+.12f*t);
                line(g,(int)(c.x+tail*vx),(int)(c.y+tail*vy),
                     (int)(c.x+progress*vx),(int)(c.y+progress*vy),1,alpha(fileColors[i%8],opacity));
                if (i%3==0) dot(g,(int)(c.x+progress*vx),(int)(c.y+progress*vy),2,alpha(accent,opacity));
            }
        }
    }
    if (flight>=0 && effect==BURST && trip>.27f && trip<.70f) {
        float t=(trip-.27f)/.43f;
        ellipseLine(g,c,18+43*t,14+29*t,2,alpha(accent,(int)(149*(1.f-t))));
    }
    int w=clamp(s->field.h/3,36,69);
    float open=opening ? 1.f-smooth((intro-.38f)/.52f) : .32f;
    if (flight>=0 && effect==BURST && trip>.30f && trip<.62f) open=.82f;
    ellipseFill(g,c.x,c.y+w*.32f,w*.53f,w*.10f,alpha(C(0x071722),118));
    folderShape(g,c,w,accent,open,0);
    if (skipCount || errCount) {
        int remaining=skipCount+errCount;
        W note[36]={0};
        statLine(note,COUNT(note),remaining,remaining==1?tr(M_LEFT_ONE):tr(M_LEFT_MANY));
        txt(g,note,(int)c.x-55,(int)c.y+w/3+3,110,17,F_TINY,C(0xf5cf8a),1);
    }
    if (phase==FINISHED && plan && plan->n && successCount==plan->n) {
        int bx=(int)c.x+w/3, by=(int)c.y+w/5;
        dot(g,bx,by,9,C(0x18333b));
        glyph(g,I_CHECK,bx-6,by-6,12,accent);
    }
}

static void drawDocks(H g, const SceneLayout *s, float trip) {
    int y=s->dockTop;
    U muted=C(0xb2c5d0);
    int labelY=y-(s->compact?19:24);
    txt(g,tr(M_TARGET_FOLDERS),s->field.x,labelY,s->field.w/2,18,F_SMALL,muted,0);
    if (s->dockCount < plan->ng) {
        W pager[40]={0};
        digits(pager,COUNT(pager),(U)s->dockStart+1,0); put(pager,COUNT(pager),T("–"));
        digits(pager,COUNT(pager),(U)(s->dockStart+s->dockCount),0); put(pager,COUNT(pager),tr(M_OF));
        digits(pager,COUNT(pager),(U)plan->ng,0);
        txt(g,pager,s->field.x+s->field.w/2,labelY,s->field.w/2,18,F_TINY,muted,2);
    }
    for (int j=0;j<s->dockCount;j++) {
        int id=s->dockStart+j;
        Group *group=&plan->g[id];
        U hue=dockColor(id);
        int active=flight>=0 && plan->e[flight].group==id;
        int x=s->dockLeft+j*(s->dockWidth+s->gap), w=s->dockWidth;
        U tileBg=blend(C(0x152737),hue,active?39:15);
        card(g,x,y,w,s->dockHeight,8,blend(C(0x243747),hue,active?170:46),tileBg);
        if (active) {
            box(g,x+8,y+s->dockHeight-3,w-16,2,1,alpha(hue,160));
        }
        Position mouth=dockMouth(s,id);
        int fw=w<115?24:28;
        folderShape(g,mouth,fw,hue,active?smooth(trip):.12f,0);
        int tx=(int)mouth.x+fw/2+6;
        txt(g,group->name,tx,y+4,x+w-tx-4,s->dockHeight/2,F_BOLD,WHITE,0);
        int landed=group->done - (active && trip<.93f ? 1 : 0);
        W count[52]={0};
        if (phase==READY) {
            statLine(count,COUNT(count),group->total,fileWord(group->total));
        } else {
            digits(count,COUNT(count),(U)imax(0,landed),0); put(count,COUNT(count),T(" / "));
            statLine(count,COUNT(count),group->total,fileWord(group->total));
        }
        txt(g,count,tx,y+s->dockHeight/2-1,x+w-tx-4,s->dockHeight/2,F_TINY,muted,0);
    }
}

static void stageHeader(H g, const SceneLayout *s, float trip) {
    int x=s->field.x, y=s->panel.y+(s->compact?4:8), h=23;
    U accent=sceneAccent();
    S label=tr(M_SOURCE_FOLDER);
    if (plan && plan->display && *plan->display) {
        label=baseName(plan->display);
        if (!*label) label=plan->display;
    }
    W route[540]={0};
    if (flight>=0) {
        Entry *e=&plan->e[flight];
        put(route,COUNT(route),e->name); put(route,COUNT(route),T("  →  "));
        put(route,COUNT(route),plan->g[e->group].name);
        label=route;
        accent=fileColors[e->cat];
    }
    dot(g,x+5,y+h/2,3,accent);
    txt(g,label,x+15,y,s->field.w-155,h,F_STAT,C(0xe7eff1),0);
    if (plan && plan->n) {
        W count[60]={0};
        int waiting=plan->n-doneCount;
        if (phase==FINISHED) {
            int left=plan->n-successCount;
            statLine(count,COUNT(count),left,left==1?tr(M_LEFT_ONE):tr(M_LEFT_MANY));
        } else if (opening && effect==BURST && trip==0.f) {
            put(count,COUNT(count),tr(M_SCATTER));
        } else {
            statLine(count,COUNT(count),waiting,tr(M_PENDING_LOW));
        }
        txt(g,count,x+s->field.w-137,y,137,h,F_SMALL,C(0xc7d6db),2);
    }
}

typedef struct {
    int index, size, opacity, labelOpacity;
    float angle;
    Position at;
} FieldToken;

static FieldToken waitingToken(const SceneLayout *s, int index, int slot, U ms,
                               float intro, float trip, int entering) {
    U key=mixBits(sceneSeed ^ ((U)index+1)*0x85ebca6bu);
    Position rest=restingSpot(s,index,slot,ms), at=rest;
    float reveal=1.f;
    float tilt=((int)(key%29u)-14)*.92f;
    if (effect==ORBIT) tilt*=.52f;
    if (opening) {
        at=openingSpot(s,rest,index,intro);
        reveal=intro<.19f ? 1.f-smooth(intro/.19f) : smooth((intro-.33f)/.45f);
        if (intro>.19f) {
            float u=withinUnit((intro-.19f)/.81f);
            tilt += (key&512 ? 1.f : -1.f)*(effect==BURST?121.f:50.f)*(1.f-u)*sineTurn((int)(u*2048));
        }
    } else if (entering && speed!=2) {
        float u=withinUnit(trip/.60f);
        at=between(s->source,rest,springOut(u));
        reveal=smooth(u);
        tilt+=(1.f-u)*27.f;
    }
    int size=(int)(s->tileSize*(.36f+.64f*reveal));
    if (effect==ORBIT && !opening && !entering)
        size=(int)(size*(.93f+.13f*(at.y-s->field.y)/(float)s->field.h));
    return (FieldToken){index,size,(int)(135+120*reveal),
                        (int)(245*smooth((reveal-.68f)/.32f)),tilt,at};
}

static int overlap(Area a, Area b) {
    return a.x < b.x+b.w && b.x < a.x+a.w && a.y < b.y+b.h && b.y < a.y+a.h;
}

static Area paperBounds(FieldToken token) {
    int w=token.size, h=token.size+4;
    return (Area){(int)token.at.x-w/2-2,(int)token.at.y-h/2,w+4,h};
}

/* Keep names readable even where two perspective orbits cross. A tile always keeps its extension. */
static void drawField(H g,const SceneLayout *s,U ms,float intro,float trip) {
    FieldToken tokens[MAX_SCENE_FILES];
    Area labels[MAX_SCENE_FILES];
    int count=0,labelCount=0;
    if (phase==FINISHED) {
        for (int j=0;j<retainedCount && count<s->slots;j++) {
            tokens[count]=waitingToken(s,retainedFiles[j],count,0,1.f,0.f,0);
            count++;
        }
        for (int i=doneCount;i<plan->n && count<s->slots;i++) {
            tokens[count]=waitingToken(s,i,count,0,1.f,0.f,0);
            count++;
        }
    } else {
        int first=flight>=0 ? flight : doneCount;
        int end=first+imin(s->slots,plan->n-first);
        for (int i=first;i<end;i++) {
            if (i==flight) continue;
            int entering=phase==RUNNING && first>0 && i==first+s->slots-1;
            tokens[count++]=waitingToken(s,i,i%s->slots,ms,intro,trip,entering);
        }
    }
    for (int i=1;i<count;i++) {
        FieldToken token=tokens[i];
        int j=i;
        while (j>0 && tokens[j-1].at.y>token.at.y) {
            tokens[j]=tokens[j-1];
            j--;
        }
        tokens[j]=token;
    }
    for (int i=0;i<count;i++) {
        FieldToken t=tokens[i];
        fileTile(g,&plan->e[t.index],t.at,t.size,t.opacity,t.angle);
    }
    int core=clamp(s->field.h/3,36,69);
    Area origin={(int)s->source.x-core/2-3,(int)s->source.y-core/3-3,core+6,
                 2*core/3+6+((skipCount||errCount)?18:0)};
    for (int i=count-1;i>=0;i--) {
        FieldToken t=tokens[i];
        if (t.labelOpacity<45) continue;
        S name=plan->e[t.index].name;
        int maxWidth=effect==ORBIT ? imin(135,s->field.w/4) : imin(138,s->field.w/s->cols);
        int width=imin(maxWidth,len(name)*7+8);
        int x=clamp((int)t.at.x-width/2,s->field.x,s->field.x+s->field.w-width);
        Area bounds=paperBounds(t);
        for (int attempt=0;attempt<2;attempt++) {
            int y=attempt ? bounds.y-16 : bounds.y+bounds.h+1;
            Area place={x,y,width,15};
            if (y<s->field.y || y+15>s->field.y+s->field.h || overlap(place,origin)) continue;
            int blocked=0;
            for (int j=0;j<count && !blocked;j++)
                if (j!=i && overlap(place,paperBounds(tokens[j]))) blocked=1;
            for (int j=0;j<labelCount && !blocked;j++)
                if (overlap(place,labels[j])) blocked=1;
            if (blocked) continue;
            labels[labelCount++]=place;
            txt(g,name,place.x,place.y,place.w,place.h,F_TINY,
                alpha(C(0xdeebef),t.labelOpacity),1);
            break;
        }
    }
}

static void drawFlight(H g, const SceneLayout *s, U ms, float trip) {
    Entry *e=&plan->e[flight];
    Position from=restingSpot(s,flight,flight%s->slots,ms);
    Position to=dockMouth(s,e->group);
    U hue=fileColors[e->cat];
    for (int i=7;i>0;i--) {
        float tail=withinUnit(trip-i*.020f), head=withinUnit(trip-(i-1)*.020f);
        if (head<=tail) continue;
        Position a=flightSpot(s,from,to,tail), b=flightSpot(s,from,to,head);
        line(g,(int)a.x,(int)a.y,(int)b.x,(int)b.y,i>4?4:7,alpha(hue,42-i*4));
        line(g,(int)a.x,(int)a.y,(int)b.x,(int)b.y,i>4?2:3,alpha(hue,210-i*23));
    }
    Position at=flightSpot(s,from,to,trip);
    float tuck=smooth((trip-.71f)/.29f);
    int size=(int)(s->tileSize*(1.f-.78f*tuck)+5*sineTurn((int)(trip*2048)));
    ellipseFill(g,at.x,at.y,size*.64f,size*.59f,alpha(hue,30));
    U seed=mixBits(sceneSeed ^ ((U)flight+1)*0x85ebca6bu);
    float tilt=((int)(seed%29u)-14)*.92f*(effect==ORBIT?.52f:1.f);
    float bank=(from.x<to.x?1.f:-1.f)*18.f*sineTurn((int)(trip*2048));
    fileTile(g,e,at,size,(int)(255*(1.f-.65f*tuck)),tilt*(1.f-trip)+bank);
    if (trip>.88f) {
        float u=(trip-.88f)/.12f;
        int fw=s->dockWidth<115?24:28;
        U dock=dockColor(e->group);
        folderShape(g,to,fw,dock,1.f,1);
        ellipseLine(g,to,13.f+8.f*u,9.f+4.f*u,1,alpha(dock,(int)(149*(1.f-u))));
    }
}

static void motionView(H g, Area panel, U now) {
    SceneLayout s=sceneLayout(panel);
    U ms=phase==RUNNING ? now-sceneEpoch : 0;
    U introLength=openingDuration(), tripLength=flightDuration();
    float intro=opening && introLength ? withinUnit((now-openingTime)/(float)introLength) : 1.f;
    float trip=flight>=0 && tripLength ? withinUnit((now-flightTime)/(float)tripLength) : 0.f;
    stageBackdrop(g,panel);
    stageHeader(g,&s,trip);
    if (!plan || !plan->n || plan->error) {
        Position center={panel.x+panel.w*.5f,panel.y+panel.h*.43f};
        ellipseFill(g,center.x,center.y,43.f,31.f,alpha(sceneAccent(),19));
        folderShape(g,center,51,sceneAccent(),.25f,0);
        S title=phase==SCAN ? tr(M_READING) :
                (plan && plan->error ? tr(M_UNAVAILABLE) : tr(M_NO_FILES));
        txt(g,title,panel.x+12,(int)center.y+32,panel.w-24,28,F_EMPTY,WHITE,1);
        txt(g,phase==SCAN?tr(M_PREPARING):tr(M_FOLDER_OR_DEMO),
            panel.x+10,(int)center.y+60,panel.w-20,23,F_DIM,C(0xb8cbd4),1);
        return;
    }
    drawEnvironment(g,&s,intro,trip);
    drawField(g,&s,ms,intro,trip);
    drawDocks(g,&s,trip);
    if (flight>=0 && s.slots>0) drawFlight(g,&s,pickupElapsed,trip);
}
