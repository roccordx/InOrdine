from pathlib import Path
import struct,io
from PIL import Image,ImageDraw
def manifest():
    return '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
 <assemblyIdentity version="1.2.0.0" processorArchitecture="amd64" name="InOrdine.Desktop" type="win32"/>
 <description>Riordino animato dei file</description>
 <dependency><dependentAssembly><assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/></dependentAssembly></dependency>
 <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3"><security><requestedPrivileges><requestedExecutionLevel level="asInvoker" uiAccess="false"/></requestedPrivileges></security></trustInfo>
 <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1"><application><supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/></application></compatibility>
 <application xmlns="urn:schemas-microsoft-com:asm.v3"><windowsSettings>
 <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
 <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2,PerMonitor</dpiAwareness>
 <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
 </windowsSettings></application>
</assembly>'''.encode()
def pad(data):return data+b'\0'*(-len(data)%4)
def vernode(key,value=b'',kind=1,length=None,children=()):
    k=(key+'\0').encode('utf-16-le');blk=pad(b'\0'*6+k)+value
    if children:
        for child in children:blk=pad(blk)+child
    n=(len(value)//2 if kind else len(value)) if length is None else length
    return struct.pack('<HHH',len(blk),n,kind)+blk[6:]
def version():
    inf=dict(FileDescription='InOrdine · Riordino animato dei file',FileVersion='1.2.0',InternalName='InOrdine',OriginalFilename='InOrdine.exe',ProductName='InOrdine',ProductVersion='1.2.0')
    tab=vernode('041004B0',children=[vernode(k,(v+'\0').encode('utf-16-le')) for k,v in inf.items()])
    fixed=struct.pack('<13I',0xFEEF04BD,0x10000,0x10002,0,0x10002,0,0x3f,0,0x40004,1,0,0,0)
    tr=vernode('Translation',struct.pack('<HH',0x410,1200),0)
    return vernode('VS_VERSION_INFO',fixed,0,children=[vernode('StringFileInfo',children=[tab]),vernode('VarFileInfo',children=[tr])])
def icons():
    size=512
    im=Image.new('RGBA',(size,size));d=ImageDraw.Draw(im)
    d.rounded_rectangle((10,10,502,502),radius=116,fill='#192d29')
    d.rounded_rectangle((84,84,428,428),radius=89,fill='#a2edbe')
    for j in range(3):
        y=170+j*76
        d.rounded_rectangle((137,y,276-j*16,y+22),11,fill='#192d29')
        d.rounded_rectangle((305-j*16,y,375,y+22),11,fill='#216947')
    sizes=[16,24,32,48,64,128,256]; group=struct.pack('<HHH',0,1,len(sizes));blobs=[]
    for i,s in enumerate(sizes,1):
        small=im.resize((s,s),Image.Resampling.LANCZOS)
        if s>=64:
            o=io.BytesIO();small.save(o,format='PNG');raw=o.getvalue()
        else:
            pix=small.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes('raw','BGRA')
            stride=((s+31)//32)*4
            mask=bytearray(stride*s)
            for y in range(s):
                for x in range(s):
                    if small.getpixel((x,s-1-y))[3]==0:mask[y*stride+x//8]|=128>>(x%8)
            raw=struct.pack('<IiiHHIIiiII',40,s,2*s,1,32,0,len(pix),0,0,0,0)+pix+mask
        blobs.append(raw);group+=struct.pack('<BBBBHHIH',s%256,s%256,0,0,1,32,len(raw),i)
    return group,blobs
def asm(base):
    grp,ic=icons();tree={3:{i+1:v for i,v in enumerate(ic)},14:{1:grp},16:{1:version()},24:{1:manifest()}}
    out=['.section .rsrc,"dr"','.balign 4'];leaf=[]
    def table(tag,entries):
        out.extend([tag+':','.long 0,0','.short 0,0,0,'+str(len(entries))])
        for idx,key,isdir in entries:out.extend(['.long '+str(idx),'.long '+key+' - rsrc_root'+(' + 0x80000000' if isdir else '')])
    table('rsrc_root',[(k,'rt'+str(k),True) for k in tree])
    for k,rows in tree.items():
        table('rt'+str(k),[(i,f'rl{k}_{i}',True) for i in rows])
        for i,b in rows.items():
            s=f'{k}_{i}';table('rl'+s,[(0x410 if k==16 else 0,'data'+s,False)]);leaf.append((s,b))
    for s,b in leaf:out.extend(['data'+s+':','.rva blob'+s,'.long '+str(len(b))+',0,0'])
    for s,b in leaf:
        p=base/'obj'/('res'+s+'.bin');p.write_bytes(b);out.extend(['.balign 4','blob'+s+':','.incbin "'+str(p)+'"'])
    return '\n'.join(out)+'\n'
