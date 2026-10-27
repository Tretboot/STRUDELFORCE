#!/usr/bin/env python3
"""
headless_verify.py — Python-Twin des C-Audio-Selbsttests (STRUDELFORCE).
Port von strudel_cleanroom.h + tfmx_macro_synth.h + test_core_audio.c.
Die Sandbox hat kein gcc/make; dieses Skript ist der verhaltensgleiche
Ersatz für `make headless_test` (Audio-Teil) und prüft die Clean-Room-
Logik deterministisch inkl. der BYO-Pipeline (user_music/).
"""
import math, os, re
MAX_NODES=128; MAX_CHILDREN=16

def parse_note_token(tok):
    if not tok or tok[0] in '~-': return -1
    base={'c':0,'d':2,'e':4,'f':5,'g':7,'a':9,'b':11}.get(tok[0].lower())
    if base is None: return -1
    i=1
    if i<len(tok) and tok[i] in '#s': base+=1; i+=1
    elif i<len(tok) and tok[i]=='b': base-=1; i+=1
    oc=3
    if i<len(tok) and tok[i].isdigit(): oc=int(tok[i])
    return (oc+1)*12+base

class Pattern:
    def __init__(s): s.nodes=[]; s.raw=""; s.root=0
    def alloc(s,t):
        if len(s.nodes)>=MAX_NODES: return 0
        s.nodes.append(dict(type=t,midi=-1,inst=0,mult=1,ek=0,en=0,children=[]))
        return len(s.nodes)-1

def atoi_lead(s):
    m=re.match(r'\s*(\d+)',s); return int(m.group(1)) if m else 0

def parse_expr(pat,s,i,parent,close):
    n=len(s)
    while i<n:
        while i<n and s[i].isspace(): i+=1
        if i>=n or s[i]==close:
            if i<n: i+=1
            return i
        if s[i] in '[<':
            exp=']' if s[i]=='[' else '>'; t='SEQ' if s[i]=='[' else 'ALT'
            ci=pat.alloc(t)
            if len(pat.nodes[parent]['children'])<MAX_CHILDREN: pat.nodes[parent]['children'].append(ci)
            i=parse_expr(pat,s,i+1,ci,exp)
            if i<n and s[i]=='*':
                i+=1; j=i
                while j<n and s[j].isdigit(): j+=1
                if j>i: pat.nodes[ci]['mult']=atoi_lead(s[i:j])
                i=j
        else:
            j=i
            while j<n and not s[j].isspace() and s[j] not in ']>[<': j+=1
            full=s[i:j]; i=j
            ai=pat.alloc('ATOM')
            if len(pat.nodes[parent]['children'])<MAX_CHILDREN: pat.nodes[parent]['children'].append(ai)
            nd=pat.nodes[ai]; star=full.find('*'); paren=full.find('('); tok=full
            if star!=-1: nd['mult']=atoi_lead(full[star+1:]); tok=full[:star]
            if paren!=-1:
                m=re.match(r'\s*(\d+)\s*,\s*(\d+)',full[paren+1:])
                if paren<len(tok): tok=tok[:paren]
                if m and int(m.group(2))>0: nd['type']='EUCLID'; nd['ek']=int(m.group(1)); nd['en']=int(m.group(2))
            nd['midi']=parse_note_token(tok)
    return i

def compile_pattern(code,di=0):
    p=Pattern(); p.raw=code[:511]; p.root=p.alloc('SEQ')
    parse_expr(p,code,0,p.root,'\x00')
    for nd in p.nodes: nd['inst']=di
    return p

def query_step(p,cyc,sin,tot):
    cur=p.root; loc=sin; span=tot
    for _ in range(8):
        n=p.nodes[cur]
        if n['mult']>1 and span>=n['mult']: span//=n['mult']; loc%=span
        t=n['type']
        if t=='ATOM': return n['midi'] if loc==0 else -2
        if t=='EUCLID':
            if n['en']==0: return -1
            sub=span//n['en'] or 1
            if loc%sub!=0: return -2
            e=(loc//sub)%n['en']
            return n['midi'] if ((e*n['ek'])%n['en'])<n['ek'] else -1
        if t=='ALT':
            if not n['children']: return -1
            cur=n['children'][cyc%len(n['children'])]
        else:
            if not n['children']: return -1
            cs=span//len(n['children']) or 1; ci=loc//cs
            if ci>=len(n['children']): ci=len(n['children'])-1
            loc%=cs; span=cs; cur=n['children'][ci]
    return -1

def i8w(v):
    v%=256; return v-256 if v>=128 else v

def tfmx_init():
    ram=[0]*65536
    for i in range(256): ram[i]=i8w((i*2 if i<128 else (i-256)*2)-64)
    for i in range(256): ram[256+i]=i8w(int(math.sin(2*math.pi*i/256+2.2*math.sin(3*2*math.pi*i/256))*120))
    for i in range(512): ram[512+i]=i8w(int((0.5*math.sin(2*math.pi*i/512)+0.3*math.sin(2.01*2*math.pi*i/512)+0.2*math.sin(4*2*math.pi*i/512))*115))
    return {'ram':ram,'macros':[dict(wave_offset=0,wave_len=128,pwm_speed=2,arp=(0,0,0),vol_decay=1),
                                dict(wave_offset=256,wave_len=256,pwm_speed=0,arp=(0,12,7),vol_decay=2),
                                dict(wave_offset=512,wave_len=512,pwm_speed=1,arp=(0,7,12),vol_decay=0)]}

def tfmx_tick(e,v,sr):
    m=e['macros'][v['macro']&15]
    v['loop_start']=(m['wave_offset']+(v['tick']*m['pwm_speed'])%64)&(65536-1)
    v['loop_len']=m['wave_len']; arp=m['arp'][v['tick']%3]
    v['inc']=440.0*2.0**((v['note']+arp-69)/12.0)*v['loop_len']/sr*65536.0
    if v['tick']>0 and v['vol']>m['vol_decay']: v['vol']-=m['vol_decay']
    v['tick']+=1

def fmt(n):
    return ("[%d] "%n) if n>=0 else (" ~  " if n==-1 else " .  ")

def run(kit):
    o=[]; A=o.append; ok=True
    pat=compile_pattern("c2 [eb2 g2] c3*2 <bb2 g2(3,8)>")
    A("=== STRUDELFORCE Clean-Room Audio Core Self-Test (Python-Twin) ===")
    A("Live-Code Pattern: %s"%pat.raw); A("Generierte AST-Knoten: %d"%len(pat.nodes)); A("")
    exp={0:[36,-2,-2,-2,39,-2,43,-2,48,-2,48,-2,46,-2,-2,-2],
         1:[36,-2,-2,-2,39,-2,43,-2,48,-2,48,-2,43,-1,-1,43]}
    for c in range(2):
        r="".join(fmt(query_step(pat,c,s,16)) for s in range(16)); A("Cycle %d: %s"%(c,r))
        if r!="".join(fmt(x) for x in exp[c]): ok=False; A("  ^^ MISMATCH! erwartet: "+"".join(fmt(x) for x in exp[c]))
    e=tfmx_init(); A(""); A("TFMX 50Hz Macro Engine bereit. Sample-RAM initialisiert (65536 Bytes).")
    A("Bank0 (PWM-Saw) Min/Max: %d/%d | Bank1 (FM-Slap) Min/Max: %d/%d | Bank2 (GreatBath-Pad) Min/Max: %d/%d"
      % (min(e['ram'][0:128]),max(e['ram'][0:128]),min(e['ram'][256:512]),max(e['ram'][256:512]),min(e['ram'][512:1024]),max(e['ram'][512:1024])))
    v=dict(macro=0,note=48,tick=0,vol=64,loop_start=0,loop_len=128,inc=0.0)
    tfmx_tick(e,v,44100)
    A("TFMX-Tick 0: voice0 macro0 (PWM-Lead, C3=48) -> loop_start=%d, sample_inc_fp=%.1f, vol=%d"%(v['loop_start'],v['inc'],v['vol']))
    A("  => %.2f Hz @ 44.1 kHz (erwartet ~130.81 Hz)"%(v['inc']/65536.0*44100.0/v['loop_len']))
    A(""); A("--- user_music/ Live-Code-Beispiele (F3-BYO-Pipeline) ---")
    um=os.path.join(kit,"user_music"); files=sorted(os.listdir(um)) if os.path.isdir(um) else []
    if not files: A("(user_music/ nicht vorhanden - uebersprungen)")
    for fn in files:
        if not fn.endswith(".strudel"): continue
        src=open(os.path.join(um,fn),encoding='utf-8').read().strip(); p2=compile_pattern(src)
        A("%s: %d Knoten"%(fn,len(p2.nodes)))
        A("  Cycle 0: "+"".join(fmt(query_step(p2,0,s,16)) for s in range(16)))
        A("  Cycle 1: "+"".join(fmt(query_step(p2,1,s,16)) for s in range(16)))
    A(""); A("RESULT: %s"%("PASS" if ok else "FAIL"))
    return "\n".join(o),ok

if __name__=="__main__":
    kit=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    t,ok=run(kit); print(t)
    open(os.path.join(kit,"PHASE1_AUDIO_SELFTEST.txt"),"w",encoding="utf-8").write(t+"\n")
    print("[*] Report geschrieben: PHASE1_AUDIO_SELFTEST.txt")
    raise SystemExit(0 if ok else 1)
