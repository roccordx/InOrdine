"""Build the Win64 app offline from C with GCC (MS ABI), LLVM MC and PE ld."""
from pathlib import Path
import ctypes as C, subprocess as sp, re, sys
base=Path(__file__).resolve().parent
obj=base/'obj'
obj.mkdir(exist_ok=True)
llvm=C.CDLL('libLLVM-19.so')
p=C.c_void_p; c=C.c_char_p; i=C.c_int
def fn(name, result, *args):
    f=getattr(llvm,'LLVM'+name); f.argtypes=args; f.restype=result; return f
for x in ['X86TargetInfo','X86Target','X86TargetMC','X86AsmPrinter','X86AsmParser']:
    fn('Initialize'+x,None)()
def coff(asm,path):
    ctx=fn('ContextCreate',p)()
    mod=fn('ModuleCreateWithNameInContext',p,c,p)(b'win',ctx)
    triple=b'x86_64-w64-windows-gnu'; target=p(); err=c()
    if fn('GetTargetFromTriple',i,c,C.POINTER(p),C.POINTER(c))(triple,C.byref(target),C.byref(err)):raise RuntimeError(err.value)
    mach=fn('CreateTargetMachine',p,p,c,c,c,i,i,i)(target,triple,b'generic',b'',2,0,0)
    fn('SetTarget',None,p,c)(mod,triple)
    b=asm.encode();fn('SetModuleInlineAsm2',None,p,c,C.c_size_t)(mod,b,len(b))
    if fn('TargetMachineEmitToFile',i,p,p,c,i,C.POINTER(c))(mach,mod,str(path).encode(),1,C.byref(err)):raise RuntimeError(err.value)
    fn('DisposeModule',None,p)(mod);fn('DisposeTargetMachine',None,p)(mach);fn('ContextDispose',None,p)(ctx)
def comp(src):
    s=obj/(src.stem+'.s')
    flags='-std=c11 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers -mabi=ms -maccumulate-outgoing-args -mno-red-zone -fpie -fno-plt -fshort-wchar -ffreestanding -fno-stack-protector -fno-ident -fno-asynchronous-unwind-tables -fno-jump-tables -fno-shrink-wrap -fno-shrink-wrap-separate -fno-schedule-insns2 -fno-optimize-sibling-calls -fstack-usage -fno-tree-loop-distribute-patterns'.split()
    flags.remove('-fno-plt')
    sp.run(['gcc','-S',*flags,'-o',s,src],check=True)
    raw=s.read_text().splitlines(); out=[]; common=[]; functions=set()
    for line in raw:
        m=re.search(r'\.type\s+([^,]+),\s*@function',line)
        if m:functions.add(m.group(1))
    for line in raw:
        t=line.strip()
        if t.startswith(('.file','.type','.size','.local','.hidden','.ident')):continue
        if t.startswith('.section'):
            name=t.split()[1].split(',')[0]
            if name.startswith('.note'):continue
            if name.startswith(('.rodata','.data.rel.ro')):line='.section .rdata,"dr"'
            elif name.startswith('.data'):line='.data'
        if t.startswith('.comm'):
            name,sz,al=t.split(None,1)[1].split(',')
            common.extend(['.balign '+al,name+':','.zero '+sz]);continue
        line=line.replace('@PLT','')
        if '@GOTPCREL' in line:raise RuntimeError('Unexpected GOT reference: '+line)
        out.append(line)
    # All internal functions use the Win64 calling convention. Describe their
    # compiler-generated prologues in x64 unwind metadata for stack walkers.
    final=[]; active=False; prolog=False
    for idx,line in enumerate(out):
        t=line.strip()
        if t.endswith(':') and t[:-1] in functions:
            if active:final.append('.seh_endproc')
            final.extend(['.seh_proc '+t[:-1],line]);active=True;prolog=True;continue
        if active and (t.startswith(('.section','.text','.data','.bss','.globl'))):
            if prolog:final.append('.seh_endprologue');prolog=False
            final.append('.seh_endproc');active=False
        if prolog:
            pushed=re.fullmatch(r'pushq\s+%(r(?:bx|bp|di|si|1[2-5]))',t)
            stack=re.fullmatch(r'subq\s+\$(\d+),\s*%rsp',t)
            saved=re.fullmatch(r'mov(?:aps|dqa|ups)\s+%(xmm(?:[6-9]|1[0-5])),\s*(\d*)\(%rsp\)',t)
            if pushed:final.extend([line,'.seh_pushreg '+pushed[1]]);continue
            if stack:final.extend([line,'.seh_stackalloc '+stack[1]]);continue
            if saved:final.extend([line,'.seh_savexmm '+saved[1]+', '+(saved[2] or '0')]);continue
            final.append('.seh_endprologue');prolog=False
        final.append(line)
    if active:final.append('.seh_endproc')
    final+=['.bss']+common
    data='\n'.join(final)+'\n';(obj/(src.stem+'.win.s')).write_text(data)
    target=obj/(src.stem+'.obj');coff(data,target);return target
def implib(dll,names):
    # A local definition-only DLL produces the standard import library for a Windows system DLL.
    # Only its import library is linked into the executable.
    defs=obj/(dll+'.def')
    defs.write_text('LIBRARY '+dll+'.dll\nEXPORTS\n'+'\n'.join(names)+'\n')
    stub=obj/(dll+'.obj')
    coff('.text\n'+''.join('.globl '+name+'\n'+name+':\nret\n' for name in names),stub)
    lib=obj/('lib'+dll+'.a')
    sp.run(['ld','-mi386pep','--dll','-e','0','--no-insert-timestamp','--out-implib',lib,'-o',obj/(dll+'.dll'),stub,defs],check=True)
    return lib
def app():
    import resources
    rs=obj/'resources.obj';coff(resources.asm(base),rs)
    source=base/'src'/'main.c'
    target=comp(source)
    und=sp.check_output(['nm','-u',target],text=True).splitlines()
    names=[l.split()[-1] for l in und]
    libs={}
    for name in names:
        if name.startswith('Gdip'):dll='GDIPLUS'
        elif name in ('SHGetKnownFolderPath','SHGetFileInfoW','SHCreateItemFromParsingName','DragQueryFileW','DragAcceptFiles','DragFinish','CommandLineToArgvW'):dll='SHELL32'
        elif name in ('CoCreateInstance','CoInitializeEx','CoUninitialize','CoTaskMemFree'):dll='OLE32'
        elif name=='ChooseColorW':dll='COMDLG32'
        elif name=='InitCommonControlsEx':dll='COMCTL32'
        elif name in ('RegGetValueW','RegCreateKeyExW','RegSetValueExW','RegCloseKey'):dll='ADVAPI32'
        elif name in ('CreateCompatibleDC','CreateCompatibleBitmap','SelectObject','DeleteObject','DeleteDC','BitBlt','CreateFontW','CreateSolidBrush'):dll='GDI32'
        elif name in ('LoadLibraryExW','GetProcAddress','GetProcessHeap','HeapAlloc','HeapReAlloc','HeapFree','GetLastError','GetModuleFileNameW','GetModuleHandleW','ExitProcess','GetTickCount','GetFileAttributesW','GetFileAttributesExW','FindFirstFileW','FindNextFileW','FindClose','CreateDirectoryW','CreateFileW','GetFileInformationByHandle','SetFileInformationByHandle','CloseHandle','FormatMessageW','CompareStringOrdinal','LCMapStringW','FileTimeToSystemTime','SystemTimeToTzSpecificLocalTime','CreateThread','CreateEventW','SetEvent','WaitForSingleObject','GetCommandLineW','LocalFree','GetFullPathNameW'):dll='KERNEL32'
        elif name[0].isupper():dll='USER32'
        else:raise ValueError('Unresolved compiler symbol '+name)
        libs.setdefault(dll,[]).append(name)
    dep=[implib(d,sorted(n)) for d,n in libs.items()]
    dest=base/'build'/'InOrdine.exe'
    dest.parent.mkdir(exist_ok=True)
    sp.run(['ld','-mi386pep','-e','start','--no-insert-timestamp','--subsystem','windows:6.0','--dynamicbase','--nxcompat','--high-entropy-va','--stack','4194304,131072','-s','-o',dest,target,rs,*dep],check=True)
if __name__=='__main__': app()
