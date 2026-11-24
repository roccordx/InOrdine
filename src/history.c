/* The Windows SQLite library is loaded once, before starting worker threads. */
#define DB_FUNCTIONS(X) \
    X(int, open16, (const void*, H*)) \
    X(int, close, (H)) \
    X(int, busy_timeout, (H,int)) \
    X(int, exec, (H,const char*,int(*)(H,int,char**,char**),H,char**)) \
    X(int, prepare_v2, (H,const char*,int,H*,const char**)) \
    X(int, step, (H)) \
    X(int, reset, (H)) \
    X(int, finalize, (H)) \
    X(int, bind_int, (H,int,int)) \
    X(int, bind_int64, (H,int,int64_t)) \
    X(int, bind_text16, (H,int,const void*,int,void(*)(void*))) \
    X(int, column_int, (H,int)) \
    X(int64_t, column_int64, (H,int)) \
    X(const void*, column_text16, (H,int)) \
    X(int64_t, last_insert_rowid, (H))
typedef struct {
#define DECL(result,name,args) result (*name) args;
    DB_FUNCTIONS(DECL)
#undef DECL
} SqlApi;
static SqlApi db;
static int dbAvailable;
static Str historyPath;
enum { DB_ROW=100, DB_DONE=101, VERIFY_FILE=4 };
enum { SESSION_OPEN, SESSION_DONE, SESSION_STOPPED, SESSION_ERROR, SESSION_PARTIAL };
static int loadDatabase(void) {
    H dll = LoadLibraryExW(T("winsqlite3.dll"), 0, 0x800);
    if (!dll) return 0;
#define LOAD(result,name,args) db.name = (result (*) args)GetProcAddress(dll,"sqlite3_" #name); if (!db.name) return 0;
    DB_FUNCTIONS(LOAD)
#undef LOAD
    return 1;
}
static void initHistory(void) {
    static const Guid localData = {0xf1b32785,0x6fba,0x4fcf,{0x9d,0x55,0x7b,0x8e,0x7f,0x15,0x70,0x91}};
    Str basePath = 0;
    dbAvailable = loadDatabase();
    if (SHGetKnownFolderPath(&localData,0,0,&basePath)<0 || !basePath) return;
    Str dir = pathjoin(basePath,T("InOrdine"));
    CoTaskMemFree(basePath);
    historyDirectory = extended(dir);
    historyPath = pathjoin(dir,T("history.sqlite3"));
    drop(dir);
    H h = CreateFileW(historyDirectory, ATTR_READ, SHARE_ALL, 0, OPEN_EXISTING, DIR_OPEN|OPEN_LINK, 0);
    if (h != BAD) {
        GetFileInformationByHandle(h, &historyDirectoryId);
        CloseHandle(h);
    }
}
static int sql(H conn, const char *statement) {
    return db.exec(conn,statement,0,0,0)==0;
}
static H prepare(H conn, const char *statement) {
    H st = 0;
    if (db.prepare_v2(conn,statement,-1,&st,0)) return 0;
    return st;
}
/* Bound strings remain valid through step/reset. Make SQLite own them to allow
   a statement to be reused independently of the lifetime of an animation. */
static int bindText(H st, int pos, S value) {
    return db.bind_text16(st,pos,value ? value:T(""),-1,(void(*)(void*))(intptr_t)-1)==0;
}
static int writeStep(H st) {
    int code = db.step(st);
    int resetCode = db.reset(st);
    return code == DB_DONE && resetCode == 0;
}
static H connectHistory(int write, int *missing) {
    if (missing) *missing=0;
    if (!dbAvailable || !historyPath) return 0;
    if (write) {
        if (!CreateDirectoryW(historyDirectory,0) && GetLastError()!=183) return 0;
    } else if (GetFileAttributesW(historyPath)==0xffffffffu) {
        U ec=GetLastError();
        if (missing && (ec==2 || ec==3)) *missing=1;
        return 0;
    }
    H conn=0;
    if (db.open16(historyPath,&conn)) {
        if (conn) db.close(conn);
        return 0;
    }
    db.busy_timeout(conn,write?3500:700);
    return conn;
}
static const char createSchema[] =
    "BEGIN IMMEDIATE;"
    "CREATE TABLE IF NOT EXISTS runs("
    "id INTEGER PRIMARY KEY, started TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, finished TEXT,"
    "root TEXT NOT NULL, method INTEGER NOT NULL, policy INTEGER NOT NULL, planned INTEGER NOT NULL,"
    "moved INTEGER DEFAULT 0, skipped INTEGER DEFAULT 0, errors INTEGER DEFAULT 0,"
    "state INTEGER DEFAULT 0, note TEXT DEFAULT '');"
    "CREATE TABLE IF NOT EXISTS files("
    "run INTEGER NOT NULL, seq INTEGER NOT NULL, name TEXT NOT NULL, folder TEXT NOT NULL,"
    "actual TEXT DEFAULT '', status INTEGER DEFAULT 0, note TEXT DEFAULT '',"
    "PRIMARY KEY(run,seq)) WITHOUT ROWID;"
    "CREATE TRIGGER IF NOT EXISTS count_results AFTER UPDATE OF status ON files "
    "WHEN NEW.status BETWEEN 1 AND 3 AND OLD.status NOT BETWEEN 1 AND 3 BEGIN "
    "UPDATE runs SET moved=moved+(NEW.status=1),skipped=skipped+(NEW.status=2),"
    "errors=errors+(NEW.status=3) WHERE id=NEW.run; END;"
    "COMMIT;";
typedef struct {
    H conn, before, after;
    int64_t id;
    int fault;
} Journal;
static void beginJournal(Journal *j, Plan *p, int policy) {
    j->fault=1;
    H conn=j->conn=connectHistory(1,0);
    if (!conn) return;
    if (!sql(conn,"PRAGMA journal_mode=WAL") || !sql(conn,createSchema) ||
        !sql(conn,"BEGIN IMMEDIATE")) {
        sql(conn,"ROLLBACK");
        db.close(conn);
        j->conn=0;
        return;
    }
    H st=prepare(conn,"INSERT INTO runs(root,method,policy,planned) VALUES(?,?,?,?)");
    int ok=st && bindText(st,1,p->display) && !db.bind_int(st,2,p->method) &&
           !db.bind_int(st,3,policy) && !db.bind_int(st,4,p->n) && writeStep(st);
    db.finalize(st);
    int64_t id=db.last_insert_rowid(conn);
    st=prepare(conn,"INSERT INTO files(run,seq,name,folder) VALUES(?,?,?,?)");
    ok=ok && st && !db.bind_int64(st,1,id);
    for (int i=0; ok && i<p->n; i++) {
        Entry *e=p->e+i;
        ok=!db.bind_int(st,2,i) && bindText(st,3,e->name) &&
           bindText(st,4,p->g[e->group].name) && writeStep(st);
    }
    db.finalize(st);
    if (!ok || !sql(conn,"COMMIT")) {
        sql(conn,"ROLLBACK");
        db.close(conn);
        j->conn=0;
        return;
    }
    j->id=id;
    j->before=prepare(conn,"UPDATE files SET status=4 WHERE run=? AND seq=?");
    j->after=prepare(conn,"UPDATE files SET status=?,actual=?,note=? WHERE run=? AND seq=?");
    j->fault=!j->before || !j->after || db.bind_int64(j->before,1,id) || db.bind_int64(j->after,4,id);
}
static void journalBefore(Journal *j, int seq) {
    if (!j->fault) j->fault=db.bind_int(j->before,2,seq) || !writeStep(j->before);
}
static void journalAfter(Journal *j, Result *r) {
    if (!j->fault) j->fault=db.bind_int(j->after,1,r->status) || !bindText(j->after,2,r->actual) ||
        !bindText(j->after,3,r->note) || db.bind_int(j->after,5,r->idx) || !writeStep(j->after);
}
static void endJournal(Journal *j, int state, S error) {
    if (!j->conn) return;
    db.finalize(j->before);
    db.finalize(j->after);
    if (j->id) {
        H st=prepare(j->conn,"UPDATE runs SET state=?,note=?,finished=CURRENT_TIMESTAMP WHERE id=?");
        if (!st || db.bind_int(st,1,j->fault?SESSION_PARTIAL:state) || !bindText(st,2,error) ||
            db.bind_int64(st,3,j->id) || !writeStep(st)) j->fault=1;
        db.finalize(st);
    }
    db.close(j->conn);
}

typedef struct {
    int64_t id;
    Str date, root, note;
    int method, planned, moved, skipped, errors, state;
} PastRun;
typedef struct {
    Str name, folder, actual, note;
    int status;
} PastFile;
typedef struct {
    PastRun *runs;
    PastFile *files;
    int runCount,fileCount,selected,readError;
} HistoryView;
static void freePastFiles(HistoryView *v) {
    for (int i=0; i<v->fileCount; i++) {
        PastFile *f=v->files+i;
        drop(f->name); drop(f->folder); drop(f->actual); drop(f->note);
    }
    drop(v->files); v->files=0; v->fileCount=0; v->selected=-1;
}
static void freeHistoryView(HistoryView *v) {
    freePastFiles(v);
    for (int i=0; i<v->runCount; i++) {
        drop(v->runs[i].date); drop(v->runs[i].root); drop(v->runs[i].note);
    }
    drop(v->runs); v->runs=0; v->runCount=0; v->readError=0;
}
static Str columnString(H st, int colIndex) {
    return dupw((S)db.column_text16(st,colIndex));
}
static void readRuns(HistoryView *v) {
    freeHistoryView(v);
    int missing=0;
    H conn=connectHistory(0,&missing);
    if (!conn) { v->readError=!missing; return; }
    H st=prepare(conn,"SELECT id,datetime(started,'localtime'),root,method,planned,moved,skipped,errors,state,note "
                      "FROM runs ORDER BY id DESC");
    int cap=0, code=-1;
    while (st && (code=db.step(st))==DB_ROW) {
        if (v->runCount==cap) {
            cap=cap?cap*2:16;
            v->runs=grow(v->runs,(size_t)cap*sizeof(PastRun));
        }
        PastRun *r=v->runs+v->runCount++;
        r->id=db.column_int64(st,0); r->date=columnString(st,1); r->root=columnString(st,2);
        r->method=db.column_int(st,3); r->planned=db.column_int(st,4); r->moved=db.column_int(st,5);
        r->skipped=db.column_int(st,6); r->errors=db.column_int(st,7); r->state=db.column_int(st,8);
        r->note=columnString(st,9);
    }
    db.finalize(st); db.close(conn);
    v->readError=code!=DB_DONE;
}
static void readRunFiles(HistoryView *v, int index) {
    freePastFiles(v);
    if (index<0 || index>=v->runCount) return;
    v->selected=index;
    H conn=connectHistory(0,0);
    if (!conn) { v->readError=1; return; }
    H st=prepare(conn,"SELECT name,folder,actual,status,note FROM files WHERE run=? ORDER BY seq");
    int cap=0, code=-1;
    int bound=st && !db.bind_int64(st,1,v->runs[index].id);
    while (bound && (code=db.step(st))==DB_ROW) {
        if (v->fileCount==cap) {
            cap=cap?cap*2:64;
            v->files=grow(v->files,(size_t)cap*sizeof(PastFile));
        }
        PastFile *f=v->files+v->fileCount++;
        f->name=columnString(st,0); f->folder=columnString(st,1); f->actual=columnString(st,2);
        f->status=db.column_int(st,3); f->note=columnString(st,4);
    }
    db.finalize(st); db.close(conn);
    v->readError=code!=DB_DONE;
}
/* Translate stored application messages on display. Keep original OS diagnostics verbatim. */
static S localizedNote(S note) {
    if (!note || !*note) return T("");
    static const int known[]={M_PLAIN_FOLDER,M_FILE_GONE,M_OWN_EXE,M_FILE_CHANGED,
                             M_LONG_NAME,M_ALREADY_EXISTS,M_NO_FREE_NAME,M_FOLDER_CHANGED};
    for (int i=0; i<COUNT(known); i++) for (int l=0; l<LANG_COUNT; l++)
        if (eq(note,messages[known[i]][l])) return tr(known[i]);
    return note;
}
static S pastState(PastRun *r) {
    static const int names[]={M_UNFINISHED,M_COMPLETE,M_INTERRUPTED,M_WITH_ERRORS,M_HISTORY_WRITE_ERROR};
    return tr(r->state>=0 && r->state<COUNT(names) ? names[r->state] : M_UNFINISHED);
}
static S pastStatus(PastFile *f, int state) {
    switch (f->status) {
        case MOVED: return tr(eq(f->name,f->actual)?M_MOVED:M_RENAMED);
        case SKIPPED: return tr(M_SKIPPED);
        case FAILED: return tr(M_ERROR);
        case PENDING: return tr(state==SESSION_PARTIAL?M_VERIFY:(state==SESSION_OPEN?M_WAITING:M_UNPROCESSED));
        default: return tr(M_VERIFY);
    }
}
