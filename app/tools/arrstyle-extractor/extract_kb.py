#!/usr/bin/env python3
"""
extract_kb.py — deterministic, dependency-free Knowledge Base extractor for the
arranger style corpus under projects/resources/.

Reads Yamaha SFF (.sty/.sst), Korg (.prs — best-effort), and plain SMF (.mid)
files, parses the embedded Standard MIDI File, and derives an ABSTRACT,
generation-oriented KB: sections, roles, rhythm cells, bass degree-movement,
chord comping rhythm, drum grooves, velocity/density profiles — never raw copies.

Deterministic by construction: no randomness, sorted iteration, integer math.
Every input file produces exactly one entry with a parse_status. Original files
are never modified.

Run:  python3 kb/tools/extract_kb.py            (from projects/resources/)
Out:  kb/styles/{discovery-*,extracted/,summaries/,aggregate/,schema/,report.md,
                 knowledge-log.md}
"""
import os, sys, json, struct, glob, hashlib, collections, math

ROOT = ""  # corpus root (set from --corpus in main)
KB   = ""  # KB output dir (set from --out in main)

# ---------------------------------------------------------------- SMF parsing
class Smf:
    __slots__ = ("fmt","ntrks","div","markers","tempos","timesigs","keysigs",
                 "notes","programs","ccs","pitchbend","ok","err")
    def __init__(self):
        self.fmt=self.ntrks=self.div=0
        self.markers=[]; self.tempos=[]; self.timesigs=[]; self.keysigs=[]
        self.notes=[]        # (tick, chan, pitch, vel, dur)
        self.programs=[]     # (chan, prog)
        self.ccs=set(); self.pitchbend=False
        self.ok=False; self.err=""

def _vlq(b, i):
    v=0
    while True:
        c=b[i]; i+=1; v=(v<<7)|(c&0x7f)
        if not c&0x80: break
    return v,i

def parse_smf(b):
    s=Smf()
    try:
        off=b.find(b"MThd")
        if off<0:
            s.err="no MThd"; return s
        s.fmt,s.ntrks,s.div = struct.unpack(">HHH", b[off+8:off+14])
        if s.div==0: s.div=480
        p=off+14
        on={}  # (chan,pitch)->(tick,vel)
        tracks_read=0
        while p+8<=len(b) and tracks_read<s.ntrks:
            if b[p:p+4]!=b"MTrk":
                # In SFF the SMF tracks come first; stop at the first non-MTrk.
                break
            tlen=struct.unpack(">I", b[p+4:p+8])[0]
            p+=8; end=min(p+tlen, len(b))
            tick=0; status=0
            while p<end:
                dt,p=_vlq(b,p); tick+=dt
                if p>=end: break
                c=b[p]
                if c&0x80: status=c; p+=1
                else: c=status
                et=c&0xf0; ch=c&0x0f
                if c==0xff:
                    mt=b[p]; p+=1; ln,p=_vlq(b,p); data=b[p:p+ln]; p+=ln
                    if mt==0x51 and ln==3:
                        s.tempos.append((tick,(data[0]<<16)|(data[1]<<8)|data[2]))
                    elif mt==0x58 and ln>=2:
                        s.timesigs.append((tick,data[0],1<<data[1]))
                    elif mt==0x59 and ln>=2:
                        s.keysigs.append((tick, struct.unpack("b",bytes([data[0]]))[0], data[1]))
                    elif mt==0x06:
                        try: s.markers.append((tick,data.decode("latin-1").strip()))
                        except Exception: pass
                elif c in (0xf0,0xf7):
                    ln,p=_vlq(b,p); p+=ln
                elif et==0x90:
                    pit=b[p]; vel=b[p+1]; p+=2
                    if vel>0: on[(ch,pit)]=(tick,vel)
                    else:
                        st=on.pop((ch,pit),None)
                        if st: s.notes.append((st[0],ch,pit,st[1],tick-st[0]))
                elif et==0x80:
                    pit=b[p]; p+=2
                    st=on.pop((ch,pit),None)
                    if st: s.notes.append((st[0],ch,pit,st[1],tick-st[0]))
                elif et==0xa0: p+=2
                elif et==0xb0:
                    s.ccs.add(b[p]); p+=2
                elif et==0xc0:
                    s.programs.append((ch,b[p])); p+=1
                elif et==0xd0: p+=1
                elif et==0xe0:
                    s.pitchbend=True; p+=2
                else:
                    p+=1
            p=end; tracks_read+=1
        # close dangling notes at their onset (len 0)
        for (ch,pit),(t,v) in on.items():
            s.notes.append((t,ch,pit,v,0))
        s.notes.sort()
        s.ok = len(s.notes)>0 or len(s.markers)>0
        return s
    except Exception as e:
        s.err=f"{type(e).__name__}: {e}"; return s

# ---------------------------------------------------------------- taxonomy
SECTION_MAP = [
    ("Intro","intro"),("Main","main"),("Fill In","fill"),("Fill","fill"),
    ("Ending","ending"),("Break","break"),
]
def section_type(name):
    for pref,typ in SECTION_MAP:
        if name.startswith(pref): return typ
    return "unknown"

def is_style_marker(name):
    return section_type(name)!="unknown"

# Yamaha SFF canonical channel -> role (0-based MIDI channel)
CHAN_ROLE = {8:"PERCUSSION",9:"DRUMS",10:"BASS",11:"CHORD_COMP",12:"CHORD_COMP",
             13:"PAD",14:"RIFF",15:"MELODY"}
def chan_role(ch, is_drum):
    if is_drum or ch==9: return "DRUMS"
    return CHAN_ROLE.get(ch,"UNKNOWN")

GENRES = ["bossa","samba","beat","rock","pop","funk","disco","house","swing","jazz",
    "blues","waltz","ballad","country","reggae","latin","tango","rumba","chacha",
    "cha cha","mambo","salsa","march","polka","foxtrot","shuffle","soul","rnb","r&b",
    "gospel","ballroom","techno","dance","hiphop","hip hop","bolero","beguine","calypso",
    "merengue","cumbia","arabic","indian","greek","schlager","musette","orchestra",
    "bigband","big band","dixie","twist","surf","metal","ska","trance","ambient","boogie","jive"]

def guess_genre(name):
    low=name.lower()
    for g in GENRES:
        if g.replace(" ","") in low.replace(" ","").replace("_","").replace("-",""):
            return g.replace(" ","").replace("&","n")
    return "unknown"

DEG = {0:"1",1:"b2",2:"2",3:"b3",4:"3",5:"4",6:"b5",7:"5",8:"b6",9:"6",10:"b7",11:"7"}
def pitch_degree(p):  # relative to source root C
    return DEG[p%12]

# ---------------------------------------------------------------- analysis
def tpb(div, ts):  # ticks per bar
    num,den=ts
    return int(div*4*num/den)

def rhythm_cell(onsets, bar_ticks, slots=16):
    """Fold onsets into one bar on a `slots`-grid -> sorted list of active slots."""
    if bar_ticks<=0: return []
    step=bar_ticks/slots
    s=set()
    for t in onsets:
        pos=(t % bar_ticks)/step
        s.add(int(round(pos)) % slots)
    return sorted(s)

def syncopation(cell, slots=16):
    """Fraction of onsets NOT on a quarter-note beat (slots 0,4,8,12)."""
    if not cell: return 0.0
    beats={0,4,8,12}
    off=sum(1 for c in cell if c not in beats)
    return round(off/len(cell),3)

def analyze(path):
    b=open(path,"rb").read()
    fmt = "sff" if (b"CASM" in b or b"SFF1" in b or b"SFF2" in b) else \
          ("smf" if b[:4]==b"MThd" else ("korg" if path.lower().endswith(".prs") else "unknown"))
    entry={
        "style_id": style_id(path),
        "source_file": os.path.relpath(path, ROOT),
        "sha1": hashlib.sha1(b).hexdigest(),
        "bytes": len(b),
        "format": fmt,
        "has_casm": (b"CASM" in b),
        "sff_version": ("SFF2" if b"SFF2" in b else ("SFF1" if b"SFF1" in b else None)),
        "parse_status": "failed",
        "confidence": 0.0,
        "musical_profile": {},
        "sections": [],
        "patterns": [],
        "arranger_notes": [],
        "programming_notes": [],
        "limitations": [],
    }
    if b[:4]!=b"MThd":
        entry["limitations"].append("no embedded SMF header (MThd); needs a format-specific decoder")
        return entry
    s=parse_smf(b)
    if not s.ok:
        entry["parse_status"]="failed"; entry["limitations"].append(s.err or "empty SMF")
        return entry
    div=s.div
    ts=(s.timesigs[0][1],s.timesigs[0][2]) if s.timesigs else (4,4)
    bar_ticks=tpb(div,ts)
    tempo_bpm = round(60000000/s.tempos[0][1]) if s.tempos else None
    key=None
    if s.keysigs:
        sf,mi=s.keysigs[0]; key=f"{sf:+d}{'m' if mi else 'M'}"

    # ---- sections from markers
    marks=sorted([(t,n) for (t,n) in s.markers if is_style_marker(n)])
    seclist=[]
    for i,(t,n) in enumerate(marks):
        nxt = marks[i+1][0] if i+1<len(marks) else (s.notes[-1][0] if s.notes else t+bar_ticks)
        span=max(0,nxt-t)
        bars=round(span/bar_ticks,2) if bar_ticks else 0
        seclist.append({"name":n,"type":section_type(n),"start":t,"end":nxt,"bars":bars})
    # if no markers, treat whole file as one 'main'
    if not seclist and s.notes:
        seclist=[{"name":"(whole)","type":"main","start":0,"end":s.notes[-1][0]+bar_ticks,"bars":round((s.notes[-1][0]+bar_ticks)/bar_ticks,2)}]

    drum_present = any(ch==9 for (_,ch,_,_,_) in s.notes)
    prog_by_chan={ch:pr for (ch,pr) in s.programs}

    total_notes=len(s.notes)
    sec_json=[]; patterns=[]
    role_active=set()
    for sec in seclist:
        st,en=sec["start"],sec["end"]
        secnotes=[nt for nt in s.notes if st<=nt[0]<en]
        bars=max(1e-9,sec["bars"])
        by_chan=collections.defaultdict(list)
        for nt in secnotes: by_chan[nt[1]].append(nt)
        tracks=[]
        sec_density=0.0
        for ch in sorted(by_chan):
            ns=by_chan[ch]
            role=chan_role(ch, ch==9)
            role_active.add(role)
            onsets=[nt[0] for nt in ns]
            vels=[nt[3] for nt in ns]
            cell=rhythm_cell(onsets, bar_ticks)
            dens=round(len(ns)/bars,2)
            sec_density+=len(ns)
            vprofile=velocity_profile(ns, bar_ticks)
            tr={"channel":ch,"role":role,
                "program":prog_by_chan.get(ch),
                "note_count":len(ns),
                "note_density_per_bar":dens,
                "velocity_mean":round(sum(vels)/len(vels)) if vels else 0,
                "velocity_profile":vprofile,
                "pitch_low":min(nt[2] for nt in ns) if ns else None,
                "pitch_high":max(nt[2] for nt in ns) if ns else None,
                "rhythm_cell_16":cell,
                "syncopation":syncopation(cell)}
            if role=="DRUMS":
                tr["drum_map"]=drum_map(ns)
            tracks.append(tr)
            # ---- abstract patterns (only for main/fill sections, key roles)
            if sec["type"] in ("main","fill","intro","ending") and role in ("BASS","CHORD_COMP","DRUMS","RIFF","MELODY","PAD"):
                rel_ns=[(nt[0]-st,nt[1],nt[2],nt[3],nt[4]) for nt in ns]  # section-relative ticks
                patterns.append(make_pattern(role, sec["type"], rel_ns, bar_ticks, cell, div, bars))
        sj={"name":sec["name"],"type":sec["type"],"bars":sec["bars"],
            "note_count":len(secnotes),
            "density":round(sec_density/max(1,len(by_chan))/max(1,bars),2),
            "tracks":tracks}
        sec_json.append(sj)

    # ---- energy progression across Main A..D
    energy=main_energy_curve(sec_json)

    genre=guess_genre(os.path.basename(path))
    feel=infer_feel(genre, s, bar_ticks)
    prof={
        "genre":genre,
        "subgenre":None,
        "feel":feel,
        "tempo_bpm":tempo_bpm,
        "tempo_range":tempo_range(genre, tempo_bpm),
        "time_signature":f"{ts[0]}/{ts[1]}",
        "ppqn":div,
        "key_signature":key,
        "swing":estimate_swing(s, bar_ticks),
        "energy":round(min(1.0,total_notes/ max(1,len(seclist))/400),3),
        "density":round(min(1.0,total_notes/ max(1,(seclist[-1]['end'] if seclist else bar_ticks)/bar_ticks)/300),3) if seclist else 0.0,
        "complexity":round(min(1.0,len(role_active)/8),3),
        "roles_present":sorted(role_active),
        "section_count":len(seclist),
        "energy_curve_main":energy,
    }
    entry["musical_profile"]=prof
    entry["sections"]=sec_json
    entry["patterns"]=patterns
    entry["arranger_notes"]=arranger_notes(sec_json)
    # status + confidence
    partial = (not marks) or total_notes==0
    entry["parse_status"]= "ok" if (marks and total_notes>0) else ("partial" if total_notes>0 else "partial")
    conf=0.4
    if marks: conf+=0.3
    if s.tempos: conf+=0.1
    if drum_present: conf+=0.1
    if len(role_active)>=4: conf+=0.1
    entry["confidence"]=round(min(1.0,conf),2)
    if not marks: entry["limitations"].append("no SFF section markers found; sections inferred as one block")
    if fmt=="korg": entry["limitations"].append("Korg .prs: SMF best-effort; native Korg chunks not decoded")
    return entry

def velocity_profile(ns, bar_ticks, beats=4):
    if bar_ticks<=0 or not ns: return []
    buckets=[[] for _ in range(beats)]
    step=bar_ticks/beats
    for (t,_,_,v,_) in ns:
        buckets[int((t%bar_ticks)/step)%beats].append(v)
    return [round(sum(x)/len(x)) if x else 0 for x in buckets]

GM_DRUM={35:"kick",36:"kick",37:"stick",38:"snare",39:"clap",40:"snare",42:"hat_closed",
    44:"hat_pedal",46:"hat_open",49:"crash",51:"ride",53:"bell",54:"tamb",56:"cowbell",
    57:"crash2",41:"tom_lo",43:"tom_lo",45:"tom_mid",47:"tom_mid",48:"tom_hi",50:"tom_hi",
    60:"bongo_hi",61:"bongo_lo",62:"conga_mute",63:"conga_open",64:"conga_lo",70:"maraca",
    75:"claves",76:"woodblock"}
def drum_map(ns):
    h=collections.Counter()
    for (_,_,p,_,_) in ns: h[GM_DRUM.get(p,f"gm{p}")]+=1
    return dict(sorted(h.items(), key=lambda kv:(-kv[1],kv[0]))[:12])

def make_pattern(role, sectype, ns, bar_ticks, cell, div, bars=1):
    onsets=sorted(nt[0] for nt in ns)
    pid=f"{role.lower()}_{sectype}_{len(cell)}c"
    pat={"pattern_id":pid,"pattern_type":role_ptype(role),"role":role,
         "section_type":sectype,
         "abstract_representation":{},
         "mutation_axes":[],"generation_notes":[]}
    ar=pat["abstract_representation"]
    ar["rhythm_cell_16"]=cell
    ar["onsets_per_bar"]=round(len(ns)/max(1e-9,bars),2) if ns else 0
    ar["syncopation"]=syncopation(cell)
    ar["density"]=round(len(cell)/16,3)
    if role=="BASS":
        # first-bar degree movement relative to source root C
        first=[nt for nt in ns if nt[0]<bar_ticks]
        first.sort()
        ar["degrees"]=[pitch_degree(nt[2]) for nt in first][:8]
        ar["contour"]=contour([nt[2] for nt in first])
        pat["mutation_axes"]=["octave","syncopation","passing_notes","rests"]
        pat["generation_notes"]=["store as chord-relative degree movement + rhythm; resolve via NTT"]
    elif role=="CHORD_COMP":
        sizes=chord_sizes(ns, bar_ticks)
        ar["avg_voices"]=sizes
        pat["mutation_axes"]=["voicing_width","inversion","rhythm_density","stab_vs_hold"]
        pat["generation_notes"]=["store rhythm cell + voice count; realize as kChordTone stack with kLead voicing"]
    elif role=="DRUMS":
        ar["drum_map"]=drum_map(ns)
        pat["mutation_axes"]=["ghost_density","fill_intensity","hat_openness","swing"]
        pat["generation_notes"]=["store as kick/snare/hat grids (kFixed); mutate ghost notes + swing"]
    elif role in ("RIFF","MELODY","PAD"):
        first=[nt for nt in ns if nt[0]<bar_ticks*2]
        ar["contour"]=contour([nt[2] for nt in sorted(first)])
        ar["degrees"]=[pitch_degree(nt[2]) for nt in sorted(first)][:12]
        pat["mutation_axes"]=["transpose","rhythmic_augmentation","ornament","rests"]
        pat["generation_notes"]=["store as scale-degree contour; realize via NoteSource::kScaleDegree"]
    return pat

def role_ptype(role):
    return {"BASS":"bass_movement","CHORD_COMP":"chord_rhythm","DRUMS":"drum_grid",
            "PAD":"pad_sustain","RIFF":"phrase_contour","MELODY":"phrase_contour"}.get(role,"generic")

def contour(pitches):
    if len(pitches)<2: return "flat"
    ups=sum(1 for a,b in zip(pitches,pitches[1:]) if b>a)
    downs=sum(1 for a,b in zip(pitches,pitches[1:]) if b<a)
    if ups>downs*1.5: return "ascending"
    if downs>ups*1.5: return "descending"
    return "arch" if pitches[0]<max(pitches)>pitches[-1] else "wave"

def chord_sizes(ns, bar_ticks):
    by_t=collections.Counter(nt[0] for nt in ns)
    if not by_t: return 0
    return round(sum(by_t.values())/len(by_t),2)

def main_energy_curve(secs):
    e={}
    for s in secs:
        if s["type"]=="main":
            e[s["name"]]=round(s["density"],2)
    return e

def arranger_notes(secs):
    notes=[]
    types=collections.Counter(s["type"] for s in secs)
    notes.append(f"sections: "+", ".join(f"{k}×{v}" for k,v in sorted(types.items())))
    mains=[s for s in secs if s["type"]=="main"]
    if len(mains)>=2:
        dens=[s["density"] for s in mains]
        if dens==sorted(dens): notes.append("Main variations show increasing density (A<B<C<D energy ladder)")
    fills=[s for s in secs if s["type"]=="fill"]
    if fills: notes.append(f"{len(fills)} fill(s), typically 1 bar, highest drum density")
    return notes

def infer_feel(genre, s, bar_ticks):
    sw=estimate_swing(s, bar_ticks)
    if genre in ("swing","jazz","blues","shuffle","bigband","boogie","jive"): return "swing"
    if genre in ("bossa","samba","latin","rumba","chacha","mambo","salsa","bolero","beguine","tango","calypso","merengue","cumbia"): return "latin"
    if genre in ("funk",): return "funk"
    if genre in ("disco","house","dance","techno","trance"): return "four_on_floor"
    if genre in ("ballad","musette"): return "ballad"
    if sw and sw>0.58: return "swing"
    return "straight"

def estimate_swing(s, bar_ticks):
    """Best-effort: mean normalized position of offbeat-8th onsets on the hat/drum
    channel. ~0.5 = straight, ->0.66 = triplet swing. Low confidence."""
    if bar_ticks<=0: return 0.0
    eighth=bar_ticks/8
    offs=[]
    for (t,ch,p,v,d) in s.notes:
        if ch!=9: continue
        pos=(t % (eighth*2))/(eighth*2)  # 0..1 within an 8th pair
        if pos>0.25:  # candidate offbeat
            offs.append(pos)
    if len(offs)<8: return 0.0
    offs.sort()
    med=offs[len(offs)//2]
    return round(min(0.75,max(0.5,med)),3)

def tempo_range(genre, bpm):
    base={"ballad":[60,90],"bossa":[110,140],"samba":[100,130],"swing":[120,220],
        "jazz":[100,220],"funk":[95,120],"disco":[115,135],"house":[118,128],
        "rock":[100,140],"pop":[90,130],"country":[80,130],"reggae":[70,100],
        "waltz":[80,180],"tango":[110,135],"march":[110,130],"blues":[60,180]}
    if genre in base: return base[genre]
    if bpm: return [max(40,bpm-20),bpm+20]
    return [80,140]

def style_id(path):
    rel=os.path.relpath(path, ROOT)
    return hashlib.sha1(rel.encode()).hexdigest()[:12] + "_" + \
           os.path.splitext(os.path.basename(path))[0].replace(" ","_").replace("/","_")[:40]

# ---------------------------------------------------------------- driver
def main():
    global ROOT, KB
    import argparse
    ap=argparse.ArgumentParser(description="arrstyle-extractor: build a deterministic style KB from an arranger-style corpus")
    ap.add_argument("--corpus", required=True, help="root dir holding the style files (scanned recursively)")
    ap.add_argument("--out", required=True, help="KB output dir (created if missing)")
    a=ap.parse_args()
    ROOT=os.path.abspath(a.corpus); KB=os.path.abspath(a.out)
    os.makedirs(KB, exist_ok=True)
    files=[]
    for ext in ("*.sty","*.STY","*.sst","*.SST","*.prs","*.PRS","*.mid","*.MID"):
        files+=glob.glob(os.path.join(ROOT,"**",ext),recursive=True)
    files=sorted(set(f for f in files if os.path.isfile(f) and "/kb/" not in f))
    print(f"[extract_kb] {len(files)} candidate style files")

    os.makedirs(os.path.join(KB,"extracted"),exist_ok=True)
    os.makedirs(os.path.join(KB,"summaries"),exist_ok=True)
    os.makedirs(os.path.join(KB,"aggregate"),exist_ok=True)

    index=[]; stats=collections.Counter()
    genre_ct=collections.Counter(); role_ct=collections.Counter()
    section_ct=collections.Counter(); fmt_ct=collections.Counter()
    rhythm_cells=collections.Counter(); bass_templates=collections.Counter()
    chord_templates=collections.Counter(); drum_grooves=collections.Counter()
    npatterns=0; confs=[]
    for i,f in enumerate(files):
        try:
            e=analyze(f)
        except Exception as ex:
            e={"style_id":style_id(f),"source_file":os.path.relpath(f,ROOT),
               "format":"unknown","parse_status":"failed","confidence":0.0,
               "limitations":[f"extractor exception: {type(ex).__name__}: {ex}"],
               "musical_profile":{},"sections":[],"patterns":[]}
        json.dump(e, open(os.path.join(KB,"extracted",e["style_id"]+".json"),"w"),
                  indent=1, sort_keys=True)
        stats[e["parse_status"]]+=1
        fmt_ct[e.get("format","unknown")]+=1
        confs.append(e.get("confidence",0.0))
        mp=e.get("musical_profile",{})
        if mp.get("genre"): genre_ct[mp["genre"]]+=1
        for r in mp.get("roles_present",[]): role_ct[r]+=1
        for s in e.get("sections",[]): section_ct[s["type"]]+=1
        for p in e.get("patterns",[]):
            npatterns+=1
            ar=p.get("abstract_representation",{})
            cell=tuple(ar.get("rhythm_cell_16",[]))
            if p["role"]=="DRUMS":
                dm=ar.get("drum_map",{}); drum_grooves[tuple(sorted(dm.keys()))[:4]]+=1
            elif p["role"]=="BASS":
                bass_templates[tuple(ar.get("degrees",[]))[:4]]+=1
            elif p["role"]=="CHORD_COMP":
                chord_templates[cell]+=1
            if cell: rhythm_cells[cell]+=1
        index.append({"style_id":e["style_id"],"source_file":e["source_file"],
                      "format":e.get("format"),"parse_status":e["parse_status"],
                      "confidence":e.get("confidence"),
                      "genre":mp.get("genre"),"tempo":mp.get("tempo_bpm"),
                      "sections":mp.get("section_count"),"roles":mp.get("roles_present",[])})
        if (i+1)%200==0: print(f"  ..{i+1}/{len(files)}")

    json.dump(index, open(os.path.join(KB,"discovery-index.json"),"w"), indent=1)
    agg=os.path.join(KB,"aggregate")
    def top(counter,n=40,keyfmt=str):
        return [{"key":keyfmt(k),"count":v} for k,v in counter.most_common(n)]
    json.dump(dict(top(genre_ct,60)), open(os.path.join(agg,"genre-index.json"),"w"),indent=1) if False else \
        json.dump(top(genre_ct,60), open(os.path.join(agg,"genre-index.json"),"w"),indent=1)
    json.dump(top(role_ct), open(os.path.join(agg,"role-index.json"),"w"),indent=1)
    json.dump(top(section_ct), open(os.path.join(agg,"section-index.json"),"w"),indent=1)
    json.dump(top(rhythm_cells,80,lambda t:",".join(map(str,t))), open(os.path.join(agg,"rhythm-cells.json"),"w"),indent=1)
    json.dump(top(bass_templates,60,lambda t:"-".join(t)), open(os.path.join(agg,"bass-templates.json"),"w"),indent=1)
    json.dump(top(chord_templates,60,lambda t:",".join(map(str,t))), open(os.path.join(agg,"chord-comping-templates.json"),"w"),indent=1)
    json.dump(top(drum_grooves,60,lambda t:"+".join(t)), open(os.path.join(agg,"drum-grooves.json"),"w"),indent=1)
    json.dump(top(rhythm_cells,120,lambda t:",".join(map(str,t))), open(os.path.join(agg,"pattern-index.json"),"w"),indent=1)
    # fill/transition rules are derived summaries, not per-file
    json.dump({"rule":"fills are 1-bar, highest drum density, one-shot back to the returning Main",
               "observed_fill_sections":section_ct.get("fill",0)},
              open(os.path.join(agg,"fill-rules.json"),"w"),indent=1)
    json.dump({"rule":"section change quantized to bar; Main A..D = energy ladder; Intro leads in, Ending stops",
               "observed_main_sections":section_ct.get("main",0)},
              open(os.path.join(agg,"transition-rules.json"),"w"),indent=1)

    summary={
        "files_total":len(files),
        "parse_status":dict(stats),
        "formats":dict(fmt_ct),
        "avg_confidence":round(sum(confs)/len(confs),3) if confs else 0,
        "patterns_extracted":npatterns,
        "genres_top":genre_ct.most_common(20),
        "roles":dict(role_ct),
        "sections":dict(section_ct),
    }
    json.dump(summary, open(os.path.join(KB,"discovery-summary.json"),"w"), indent=1)
    print("[extract_kb] done:", json.dumps(summary["parse_status"]),
          "patterns=",npatterns, "avg_conf=",summary["avg_confidence"])
    return summary

if __name__=="__main__":
    main()
