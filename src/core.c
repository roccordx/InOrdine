#include "win.h"
#define T(x) WTEXT(x)
#include "locale.c"
enum{DIR_ATTR = 0x10, HIDDEN = 2, SYSTEM = 4, LINK = 0x400, OPEN_EXISTING = 3, SHARE_ALL = 7, ATTR_READ = 0x80, OPEN_LINK = 0x200000, DIR_OPEN = 0x2000000};
enum{EXTENSION, CATEGORY, MONTH};
enum{PENDING, MOVED, SKIPPED, FAILED};
enum{DOC, IMAGE, VIDEO, AUDIO, ARCHIVE, APP, CODE, OTHER};
static const int catNames[] = {M_CAT_DOC, M_CAT_IMAGE, M_CAT_VIDEO, M_CAT_AUDIO, M_CAT_ARCHIVE, M_CAT_APP, M_CAT_CODE, M_CAT_OTHER};
static const S catList[] = {
T("pdf doc docx xls xlsx ppt pptx odt ods odp rtf txt csv tsv md epub mobi pages numbers key xlsm docm p7m"),
T("jpg jpeg png gif bmp webp heic heif avif tif tiff svg ico raw dng cr2 nef arw"),
T("mp4 mov mkv avi wmv webm m4v mpg mpeg mts m2ts 3gp"),
T("mp3 wav flac aac m4a ogg opus wma aiff mid midi"),
T("zip 7z rar tar gz tgz bz2 xz iso cab zst"),
T("exe msi msix appx apk bat cmd ps1 reg com msp"),
T("c h cpp hpp cs py js jsx ts tsx json xml html htm css scss sql java go rs sh yml yaml toml php rb ini cfg")
};
static const int methodText[] = {M_BY_EXT, M_BY_CAT, M_BY_MONTH};
static const int conflictText[] = {M_SKIP_POLICY, M_RENAME_POLICY};
static const int speedText[] = {M_SMOOTH, M_FAST, M_INSTANT};
static H hp;
static FileInfo selfInfo;
static int knownSelf;
static Str historyDirectory;
static FileInfo historyDirectoryId;
static int sameId(FileInfo a, FileInfo b) {
    return (a.idHi || a.idLo) && a.volume == b.volume && a.idHi == b.idHi && a.idLo == b.idLo;
}
static void oom(void) {
    MessageBoxW(0, tr(M_OOM), T("InOrdine"), 16);
    ExitProcess(1);
}
static H mem(size_t n) {
    H p = HeapAlloc(hp, 8, n ? n:1);
    if (!p) oom();
    return p;
}
static H grow(H p, size_t n) {
    if (!p) return mem(n);
    H q = HeapReAlloc(hp, 8, p, n);
    if (!q) oom();
    return q;
}
static void drop(H p) {
    if (p) HeapFree(hp, 0, p);
}
static int len(S s) {
    int n = 0;
    if (s) while (s[n]) n++;
    return n;
}
static void copyw(Str d, S s, int n) {
    for (int i = 0; i<n; i++) d[i] = s[i];
}
static Str part(S s, int n) {
    Str p = mem((size_t)(n+1)*2);
    copyw(p, s, n);
    return p;
}
static Str dupw(S s) {
    return part(s, len(s));
}
static int eq(S a, S b) {
    return CompareStringOrdinal(a, -1, b, -1, 1) == 2;
}
static int starts(S s, S pre) {
    int n = len(pre);
    return len(s) >= n && CompareStringOrdinal(s, n, pre, n, 1) == 2;
}
static void put(Str d, int cap, S s) {
    int n = len(d), m = len(s);
    if (n >= cap-1) return;
    if (m>cap-1-n) m = cap-1-n;
    copyw(d+n, s, m);
    d[n+m] = 0;
}
static void digits(Str d, int cap, U val, int width) {
    W b[17];
    int i = 16;
    b[i] = 0;
    do {
        b[--i] = (W)('0'+val%10);
        val /= 10;
        width--;
    }while ((val || width>0) && i>0);
    put(d, cap, b+i);
}
static Str join(S a, S sep, S b) {
    int x = len(a), z = len(sep), y = len(b);
    Str p = mem((size_t)(x+y+z+1)*2);
    copyw(p, a, x);
    copyw(p+x, sep, z);
    copyw(p+x+z, b, y);
    return p;
}
static Str pathjoin(S a, S b) {
    int n = len(a);
    return join(a, (n && a[n-1] == '\\') ? T(""):T("\\"), b);
}
static S baseName(S s) {
    S p = s;
    for (; *s; s++) if (*s == '\\' || *s == '/') p = s+1;
    return p;
}
static Str extended(S s) {
    if (starts(s, T("\\\\?\\"))) return dupw(s);
    if (starts(s, T("\\\\"))) return join(T("\\\\?\\UNC\\"), T(""), s+2);
    return join(T("\\\\?\\"), T(""), s);
}
static Str plain(S s) {
    if (starts(s, T("\\\\?\\UNC\\"))) return join(T("\\\\"), T(""), s+8);
    if (starts(s, T("\\\\?\\"))) return dupw(s+4);
    return dupw(s);
}
static S suffix(S name) {
    S p = 0;
    for (S s = name; *s; s++) if (*s == '.') p = s;
    return p && p>name && p[1] ? p+1:T("");
}
static int token(S tokens, S ext) {
    int n = len(ext);
    while (*tokens) {
        while (*tokens == ' ') tokens++;
        S s = tokens;
        while (*tokens && *tokens != ' ') tokens++;
        if (tokens-s == n && CompareStringOrdinal(s, n, ext, n, 1) == 2) return 1;
    }
    return 0;
}
static int category(S ext) {
    for (int i = 0; i<COUNT(catList); i++) if (token(catList[i], ext)) return i;
    return OTHER;
}
static int reserved(S s) {
    if (eq(s, T("con")) || eq(s, T("prn")) || eq(s, T("aux")) || eq(s, T("nul")) || eq(s, T("conin$")) || eq(s, T("conout$")) || eq(s, T("clock$"))) return 1;
    if (len(s) == 4 && (starts(s, T("com")) || starts(s, T("lpt")))) {
        W a = s[3];
        return (a >= '0' && a <= '9') || a == 0xb9 || a == 0xb2 || a == 0xb3;
    }
    return 0;
}
static void groupName(Str g, int cap, S ext, FT date, int method) {
    if (method == MONTH) {
        ST utc, local;
        if (FileTimeToSystemTime(&date, &utc) && SystemTimeToTzSpecificLocalTime(0, &utc, &local)) {
            digits(g, cap, local.year, 4);
            put(g, cap, T("-"));
            digits(g, cap, local.month, 2);
        } else put(g, cap, tr(M_UNKNOWN_DATE));
        return;
    }
    if (reserved(ext)) put(g, cap, T("_"));
    int n = len(g);
    if (!LCMapStringW(0x7f, 0x100, ext, -1, g+n, cap-n)) put(g, cap, ext);
    for (int i = 0; g[i]; i++) if (g[i]<32 || g[i] == '<' || g[i] == '>' || g[i] == ':' || g[i] == '"' || g[i] == '/' || g[i] == '\\' || g[i] == '|' || g[i] == '?' || g[i] == '*') g[i] = '_';
    n = len(g);
    while (n && (g[n-1] == ' ' || g[n-1] == '.')) g[--n] = 0;
    if (!n) put(g, cap, T("_estensione"));
    if (reserved(g)) {
        for (int i = n; i >= 0; i--) g[i+1] = g[i];
        g[0] = '_';
    }
}
typedef struct {
    Str name;
    int total, done, category;
} Group;
typedef struct {
    Str name, actual, note;
    FT m, c;
    U hi, lo;
    int group, cat, status, clash;
} Entry;
typedef struct {
    Str root, display, error;
    Entry *e;
    Group *g;
    int n, ne, ng, nc, ignored, dirs, method, demo;
    int *slots, sc;
    FileInfo rootInfo;
    S categoryFolders[8];
} Plan;
static U hashw(S s) {
    U h = 2166136261u;
    while (*s) {
        h = (h^*s++)*16777619u;
    }
    return h;
}
static void groupIndex(Plan *p, int cap) {
    int *z = mem((size_t) cap*sizeof(int));
    for (int i = 0; i<p->ng; i++) {
        U at = hashw(p->g[i].name)&(cap-1);
        while (z[at]) at = (at+1)&(cap-1);
        z[at] = i+1;
    }
    drop(p->slots);
    p->slots = z;
    p->sc = cap;
}
static int ensureGroup(Plan *p, S name) {
    if (!p->sc || p->ng*2 >= p->sc) groupIndex(p, p->sc ? p->sc*2:32);
    U at = hashw(name)&(p->sc-1);
    while (p->slots[at]) {
        int id = p->slots[at]-1;
        if (eq(name, p->g[id].name)) return id;
        at = (at+1)&(p->sc-1);
    }
    if (p->ng == p->nc) {
        p->nc = p->nc ? p->nc*2:16;
        p->g = grow(p->g, (size_t) p->nc*sizeof(Group));
    }
    int id = p->ng++;
    p->slots[at] = id+1;
    p->g[id].name = dupw(name);
    return id;
}
/* Reuse a category directory made in another UI language, including version 1.1.
   The selected language wins when more than one matching directory exists. */
static S categoryFolder(Plan *p, int cat) {
    if (p->categoryFolders[cat]) return p->categoryFolders[cat];
    int key = catNames[cat];
    S choice = tr(key);
    if (p->root) for (int pass = 0; pass <= LANG_COUNT; pass++) {
        int lang = pass == 0 ? language : pass-1;
        if (pass && lang == language) continue;
        S candidate = messages[key][lang];
        Str path = pathjoin(p->root, candidate);
        U attributes = GetFileAttributesW(path);
        drop(path);
        if (attributes != 0xffffffffu && (attributes & (DIR_ATTR|LINK)) == DIR_ATTR) {
            choice = candidate;
            break;
        }
    }
    return p->categoryFolders[cat] = choice;
}
static Entry *add(Plan *p, S name, FT c, FT m, U hi, U lo) {
    if (p->n == p->ne) {
        p->ne = p->ne ? p->ne*2:64;
        p->e = grow(p->e, (size_t) p->ne*sizeof(Entry));
    }
    Entry *e = p->e+p->n++;
    e->name = dupw(name);
    e->m = m;
    e->c = c;
    e->hi = hi;
    e->lo = lo;
    S ext = suffix(name);
    e->cat = category(ext);
    W gn[320] = {0};
    S destination;
    if (p->method == CATEGORY) destination = categoryFolder(p, e->cat);
    else {
        groupName(gn, COUNT(gn), ext, m, p->method);
        destination = gn;
    }
    e->group = ensureGroup(p, destination);
    p->g[e->group].category = e->cat;
    p->g[e->group].total++;
    return e;
}
static void freePlan(Plan *p) {
    if (!p) return;
    for (int i = 0; i<p->n; i++) {
        drop(p->e[i].name);
        drop(p->e[i].actual);
        drop(p->e[i].note);
    }
    for (int i = 0; i<p->ng; i++) drop(p->g[i].name);
    drop(p->e);
    drop(p->g);
    drop(p->root);
    drop(p->display);
    drop(p->slots);
    drop(p->error);
    drop(p);
}
static void sink(Entry *e, int start, int end) {
    int root = start;
    while (root<end/2) {
        int child = root*2+1;
        if (child+1<end && CompareStringOrdinal(e[child].name, -1, e[child+1].name, -1, 1) == 1) child++;
        if (CompareStringOrdinal(e[root].name, -1, e[child].name, -1, 1) != 1) break;
        Entry t = e[root];
        e[root] = e[child];
        e[child] = t;
        root = child;
    }
}
static void sortEntries(Plan *p) {
    for (int i = p->n/2; i-->0;) sink(p->e, i, p->n);
    for (int i = p->n; i>1;) {
        i--;
        Entry t = p->e[0];
        p->e[0] = p->e[i];
        p->e[i] = t;
        sink(p->e, 0, i);
    }
}
static Str syserr(U ec) {
    W s[400] = {0};
    static const W ids[] = {0x0410, 0x0409, 0x0c0a, 0x040c};
    if (!FormatMessageW(0x1200, 0, ec, ids[language], s, COUNT(s), 0) &&
        !FormatMessageW(0x1200, 0, ec, 0, s, COUNT(s), 0)) {
        put(s, COUNT(s), tr(M_WIN_ERROR));
        digits(s, COUNT(s), ec, 0);
    }
    int n = len(s);
    while (n && (s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == ' ')) s[--n] = 0;
    return dupw(s);
}
/* Keep the directory open without delete sharing while validating or moving files. */
static H pin(S path, Str *error) {
    H h = CreateFileW(path, ATTR_READ, 3, 0, OPEN_EXISTING, DIR_OPEN|OPEN_LINK, 0);
    if (h == BAD) {
        *error = syserr(GetLastError());
        return 0;
    }
    FileInfo f;
    if (!GetFileInformationByHandle(h, &f)) {
        *error = syserr(GetLastError());
        CloseHandle(h);
        return 0;
    }
    if (!(f.attr&DIR_ATTR) || (f.attr&LINK)) {
        *error = dupw(tr(M_PLAIN_FOLDER));
        CloseHandle(h);
        return 0;
    }
    return h;
}
static int eligible(S name, U a, int isSelf) {
    return!(a&(DIR_ATTR|HIDDEN|SYSTEM|LINK)) && *name != '.' && *suffix(name) && !isSelf && !eq(name, T("ordine.bat")) && !eq(name, T("desktop.ini")) && !eq(name, T("thumbs.db"));
}
static void scan(Plan *p, S self) {
    Str err = 0;
    H root = pin(p->root, &err);
    if (!root) {
        p->error = err;
        return;
    }
    GetFileInformationByHandle(root, &p->rootInfo);
    Str pattern = pathjoin(p->root, T("*"));
    FindData fd;
    H h = FindFirstFileW(pattern, &fd);
    U ec = h == BAD ? GetLastError():0;
    drop(pattern);
    if (h == BAD) {
        if (ec != 2 && ec != 18) p->error = syserr(ec);
        CloseHandle(root);
        return;
    }
    int inHistoryDir = historyDirectory && (eq(p->root, historyDirectory) ||
                      sameId(p->rootInfo, historyDirectoryId));
    do {
        if (inHistoryDir && (eq(fd.name, T("history.sqlite3")) ||
            starts(fd.name, T("history.sqlite3-")))) { p->ignored++; continue; }
        if (fd.attr&DIR_ATTR) {
            if (!eq(fd.name, T(".")) && !eq(fd.name, T(".."))) p->dirs++;
            continue;
        }
        int same = 0;
        if (eq(fd.name, baseName(self))) {
            Str path = pathjoin(p->root, fd.name);
            same = eq(path, self);
            drop(path);
        }
        if (!eligible(fd.name, fd.attr, same)) {
            p->ignored++;
            continue;
        }
        Entry *e = add(p, fd.name, fd.created, fd.modified, fd.sizeHi, fd.sizeLo);
        Str dir = pathjoin(p->root, p->g[e->group].name);
        U a = GetFileAttributesW(dir);
        if (a != 0xffffffffu && ((a&LINK) || !(a&DIR_ATTR))) {
            e->clash = 2;
        }
        else if (a != 0xffffffffu) {
            Str dst = pathjoin(dir, e->name);
            if (GetFileAttributesW(dst) != 0xffffffffu) e->clash = 1;
            drop(dst);
        }
        drop(dir);
    }while (FindNextFileW(h, &fd));
    ec = GetLastError();
    FindClose(h);
    CloseHandle(root);
    if (ec != 18) p->error = syserr(ec);
    sortEntries(p);
}
static Str numbered(S name, int number) {
    S ext = suffix(name);
    int stem = (int)(ext-name)-1;
    W tag[24] = {0};
    put(tag, 24, T(" ("));
    digits(tag, 24, (U) number, 0);
    put(tag, 24, T(")"));
    int en = len(ext);
    int maxStem = 255-1-en-len(tag);
    if (maxStem<1) return 0;
    if (stem>maxStem) {
        stem = maxStem;
        if (stem && name[stem-1] >= 0xd800 && name[stem-1] <= 0xdbff) stem--;
    }
    if (!stem) return 0;
    Str head = part(name, stem);
    Str tagged = join(head, tag, T("."));
    Str out = join(tagged, T(""), ext);
    drop(head);
    drop(tagged);
    return out;
}
typedef struct {
    int idx, status;
    Str actual, note;
} Result;
static void fail(Result *r, U ec) {
    r->status = FAILED;
    r->note = syserr(ec);
}
static void skip(Result *r, S why) {
    r->status = SKIPPED;
    r->note = dupw(why);
}
static int equalft(FT a, FT b) {
    return a.hi == b.hi && a.lo == b.lo;
}
/* Rename the exact source handle checked against the preview. The kernel decides conflicts. */
static void moveOne(Plan *p, int index, int conflicts, Result *r) {
    Entry *e = &p->e[index];
    r->idx = index;
    Str src = pathjoin(p->root, e->name);
    H srcH = CreateFileW(src, 0x10000|ATTR_READ, 5, 0, OPEN_EXISTING, OPEN_LINK, 0);
    U ec = srcH == BAD ? GetLastError():0;
    drop(src);
    if (srcH == BAD) {
        if (ec == 2 || ec == 3) skip(r, tr(M_FILE_GONE));
        else fail(r, ec);
        return;
    }
    FileInfo info;
    H dirH = 0;
    Str folder = 0, why = 0;
    if (!GetFileInformationByHandle(srcH, &info)) {
        fail(r, GetLastError());
        goto end;
    }
    if (knownSelf && sameId(info, selfInfo)) {
        skip(r, tr(M_OWN_EXE));
        goto end;
    }
    if ((info.attr&(LINK|DIR_ATTR|HIDDEN|SYSTEM)) || info.sizeHi != e->hi || info.sizeLo != e->lo || !equalft(info.m, e->m) || !equalft(info.c, e->c)) {
        skip(r, tr(M_FILE_CHANGED));
        goto end;
    }
    folder = pathjoin(p->root, p->g[e->group].name);
    if (!CreateDirectoryW(folder, 0)) {
        ec = GetLastError();
        if (ec != 183) {
            fail(r, ec);
            goto end;
        }
    }
    dirH = pin(folder, &why);
    if (!dirH) {
        r->status = FAILED;
        r->note = why;
        goto end;
    }
    for (int seq = 1;; seq++) {
        Str attempt = seq == 1 ? dupw(e->name):numbered(e->name, seq);
        if (!attempt) {
            r->status = FAILED;
            r->note = dupw(tr(M_LONG_NAME));
            break;
        }
        Str dst = pathjoin(folder, attempt);
        U bytes = (U) len(dst)*2;
        U sz = (U) sizeof(RenameInfo)+bytes;
        RenameInfo *rn = mem(sz);
        rn->nameBytes = bytes;
        copyw(rn->name, dst, (int)(bytes/2));
        /* ReplaceIfExists stays false. */
        I ok = SetFileInformationByHandle(srcH, 3, rn, sz);
        ec = ok ? 0:GetLastError();
        drop(rn);
        int exists = !ok && (ec == 80 || ec == 183 || (ec == 5 && GetFileAttributesW(dst) != 0xffffffffu));
        drop(dst);
        if (ok) {
            r->status = MOVED;
            r->actual = attempt;
            break;
        }
        drop(attempt);
        if (!exists) {
            fail(r, ec);
            break;
        }
        if (!conflicts) {
            skip(r, tr(M_ALREADY_EXISTS));
            break;
        }
        if (seq == 999999) {
            r->status = FAILED;
            r->note = dupw(tr(M_NO_FREE_NAME));
            break;
        }
    }
    end:
    if (dirH) CloseHandle(dirH);
    drop(folder);
    CloseHandle(srcH);
}
