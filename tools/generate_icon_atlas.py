#!/usr/bin/env python3
"""Generate one deterministic RGBA texture atlas for the native UI.
No image decoder or runtime file I/O is required. The atlas is generated at build time.
The renderer intentionally uses compact vector-like primitives; the source mapping can later
be swapped for official Adventure Land sprites without touching the UI/IconCache API.
"""
from __future__ import annotations
import hashlib, math, sys
from pathlib import Path

CELL=32; COLS=16
CLASSES=['mage','merchant','paladin','priest','ranger','rogue','warrior']
SKILLS=['3shot','5shot','absorb','aether_shield','agitate','arcane_needle','beacon','blink','burst','cburst','charge','cleansing_light','cleave','curse','darkblessing','dash','energize','entangle','fanofknives','fishing','guardians_oath','hardshell','huntersmark','invis','magiport','massexchange','massexchangepp','massproduction','massproductionpp','mentalburst','mining','mluck','paladin_aura','partyheal','pcoat','phaseout','piercingshot','poisonarrow','purify','quickpunch','quickstab','reflection','regen_hp','regen_mp','revive','rspeed','shadowstrike','shield_slam','stomp','supershot','taunt','track','warcry']
ITEMS=['accessory','armor','armorring','computer','concordmace','consumable','cscroll0','dawnwardaegis','dkey','fireblade','firebow','firestaff','gbow','gem0','gemfragment','gloampendant','goldbooster','harbringer','hpot0','hpot1','hpotx','leather','luckbooster','marketparcel','material','monstertoken','mpot0','mpot1','mpotx','mushroomstaff','oathplate','offering','orbg','orboftemporal','pickaxe','pvptoken','quiver','resistancering','resolutesallet','rod','sanguine','scroll0','seashell','special','stand0','supercomputer','tool','tracker','vowkeepergloves','weapon','worldroot','xpbooster']
FALLBACK=['items:__fallback','items:__weapon','items:__armor','items:__accessory','items:__consumable','items:__material','items:__scroll','items:__token','items:__orb','skills:__fallback','classes:__fallback']
KEYS=[f'classes:{x}' for x in CLASSES]+[f'skills:{x}' for x in SKILLS]+[f'items:{x}' for x in ITEMS]+FALLBACK
ROWS=(len(KEYS)+COLS-1)//COLS; W=COLS*CELL; H=ROWS*CELL
buf=bytearray(W*H*4)

def px(x,y,c):
    if 0<=x<W and 0<=y<H:
        i=(y*W+x)*4; buf[i:i+4]=bytes(c)
def rect(x0,y0,x1,y1,c):
    for y in range(max(0,y0),min(H,y1)):
        for x in range(max(0,x0),min(W,x1)): px(x,y,c)
def line(x0,y0,x1,y1,c,w=1):
    dx=abs(x1-x0); sx=1 if x0<x1 else -1; dy=-abs(y1-y0); sy=1 if y0<y1 else -1; err=dx+dy
    while True:
        for yy in range(y0-w+1,y0+w):
            for xx in range(x0-w+1,x0+w): px(xx,yy,c)
        if x0==x1 and y0==y1: break
        e2=2*err
        if e2>=dy: err+=dy; x0+=sx
        if e2<=dx: err+=dx; y0+=sy
def circle(cx,cy,r,c,fill=True):
    rr=r*r
    for y in range(cy-r,cy+r+1):
        for x in range(cx-r,cx+r+1):
            d=(x-cx)**2+(y-cy)**2
            if (fill and d<=rr) or (not fill and rr-r*2<=d<=rr+r): px(x,y,c)
def diamond(cx,cy,r,c):
    for y in range(cy-r,cy+r+1):
        span=r-abs(y-cy)
        for x in range(cx-span,cx+span+1): px(x,y,c)
def hash_color(key,base):
    d=hashlib.blake2s(key.encode(),digest_size=3).digest()
    return tuple(min(255,(base[i]+d[i])//2) for i in range(3))+(255,)

def sword(cx,cy,c): line(cx-7,cy+7,cx+6,cy-6,c,2); line(cx+4,cy-8,cx+8,cy-4,c,2); line(cx-7,cy+3,cx-3,cy+7,c,2)
def shield(cx,cy,c):
    line(cx-6,cy-7,cx+6,cy-7,c,1); line(cx-6,cy-7,cx-5,cy+4,c,1); line(cx+6,cy-7,cx+5,cy+4,c,1); line(cx-5,cy+4,cx,cy+9,c,1); line(cx+5,cy+4,cx,cy+9,c,1)
def cross(cx,cy,c): rect(cx-2,cy-8,cx+3,cy+9,c); rect(cx-8,cy-2,cx+9,cy+3,c)
def arrow(cx,cy,c): line(cx-8,cy+5,cx+7,cy-5,c,2); line(cx+7,cy-5,cx+2,cy-6,c,2); line(cx+7,cy-5,cx+6,cy,c,2)
def bolt(cx,cy,c): line(cx+3,cy-9,cx-4,cy,c,2); line(cx-4,cy,cx+2,cy,c,2); line(cx+2,cy,cx-4,cy+9,c,2)
def potion(cx,cy,c): rect(cx-4,cy-7,cx+5,cy-3,c); rect(cx-7,cy-3,cx+8,cy+8,c); line(cx-6,cy+2,cx+7,cy+2,(255,255,255,220),1)
def scroll(cx,cy,c): rect(cx-7,cy-8,cx+8,cy+8,c); line(cx-4,cy-3,cx+4,cy-3,(30,40,50,255)); line(cx-4,cy+1,cx+3,cy+1,(30,40,50,255)); line(cx-4,cy+5,cx+5,cy+5,(30,40,50,255))
def bow(cx,cy,c):
    for a in range(-70,71,7):
        rad=math.radians(a); px(int(cx-5+7*math.cos(rad)),int(cy+8*math.sin(rad)),c)
    line(cx-5,cy-8,cx-5,cy+8,c,1); arrow(cx+2,cy,c)
def pick(cx,cy,c): line(cx-5,cy-8,cx+5,cy+8,c,2); line(cx-9,cy-7,cx+3,cy-7,c,2)
def icon_for(section,name,cx,cy,fg):
    n=name.lower()
    if section=='classes':
        if n=='mage': bolt(cx,cy,fg); circle(cx+6,cy-7,2,fg)
        elif n=='merchant': circle(cx,cy,8,fg,False); line(cx-3,cy,cx+3,cy,fg,1)
        elif n=='paladin': shield(cx,cy,fg); cross(cx,cy-1,fg)
        elif n=='priest': cross(cx,cy,fg)
        elif n=='ranger': bow(cx,cy,fg)
        elif n=='rogue': sword(cx-3,cy,fg); sword(cx+4,cy,fg)
        else: sword(cx,cy,fg); shield(cx+4,cy+2,fg)
        return
    if section=='skills':
        if any(x in n for x in ('heal','revive','regen','purify','cleansing')): cross(cx,cy,fg)
        elif any(x in n for x in ('shot','arrow','track','hunter')): arrow(cx,cy,fg)
        elif any(x in n for x in ('shield','absorb','shell','reflect','oath','coat')): shield(cx,cy,fg)
        elif any(x in n for x in ('dash','charge','speed','phase','blink','port')): arrow(cx,cy,fg); circle(cx-6,cy+6,3,fg,False)
        elif 'fish' in n: line(cx-7,cy-6,cx+3,cy+5,fg,1); circle(cx+5,cy+6,3,fg,False)
        elif 'mining' in n: pick(cx,cy,fg)
        elif any(x in n for x in ('knife','stab','punch','cleave','slam','stomp')): sword(cx,cy,fg)
        else: bolt(cx,cy,fg)
        return
    if any(x in n for x in ('pot','consum')): potion(cx,cy,fg)
    elif 'scroll' in n: scroll(cx,cy,fg)
    elif any(x in n for x in ('bow','quiver')): bow(cx,cy,fg)
    elif any(x in n for x in ('blade','mace','staff','weapon','harbringer','sanguine','rod')): sword(cx,cy,fg)
    elif any(x in n for x in ('armor','plate','glove','sallet')): shield(cx,cy,fg)
    elif any(x in n for x in ('ring','pendant','accessory')): circle(cx,cy,7,fg,False); circle(cx,cy,2,fg)
    elif any(x in n for x in ('token','booster','offering')): circle(cx,cy,8,fg,False); diamond(cx,cy,3,fg)
    elif 'orb' in n: circle(cx,cy,8,fg,False); circle(cx,cy,4,fg)
    elif any(x in n for x in ('pickaxe','tool')): pick(cx,cy,fg)
    elif any(x in n for x in ('computer','tracker')): rect(cx-8,cy-6,cx+9,cy+6,fg); rect(cx-3,cy+6,cx+4,cy+9,fg)
    else: diamond(cx,cy,8,fg)

def tile(i,key):
    section,name=key.split(':',1); ox=(i%COLS)*CELL; oy=(i//COLS)*CELL
    base={'classes':(38,80,115),'skills':(75,46,116),'items':(40,85,72)}[section]
    bg=hash_color(key,base); edge=tuple(min(255,x+55) for x in bg[:3])+(255,)
    rect(ox+2,oy+2,ox+30,oy+30,(12,20,29,255)); rect(ox+3,oy+3,ox+29,oy+29,bg)
    line(ox+3,oy+3,ox+28,oy+3,edge); line(ox+3,oy+3,ox+3,oy+28,edge)
    icon_for(section,name,ox+16,oy+16,(235,245,250,255))

for i,k in enumerate(KEYS): tile(i,k)
out=Path(sys.argv[1]); out.parent.mkdir(parents=True,exist_ok=True)
entries=[(k,(i%COLS)*CELL,(i//COLS)*CELL,CELL,CELL) for i,k in enumerate(KEYS)]
with out.open('w',encoding='utf-8') as f:
    f.write('#include "icon_atlas_embedded.hpp"\n\n')
    f.write(f'const int AL_ICON_ATLAS_WIDTH={W};\nconst int AL_ICON_ATLAS_HEIGHT={H};\n')
    f.write('const unsigned char AL_ICON_ATLAS_RGBA[] = {\n')
    for i in range(0,len(buf),32): f.write('  '+','.join(map(str,buf[i:i+32]))+',\n')
    f.write('};\nconst EmbeddedIconEntry AL_ICON_ENTRIES[] = {\n')
    for k,x,y,w,h in entries: f.write(f'  {{"{k}",{x},{y},{w},{h}}},\n')
    f.write('};\nconst std::size_t AL_ICON_ENTRY_COUNT=sizeof(AL_ICON_ENTRIES)/sizeof(AL_ICON_ENTRIES[0]);\n')
print(f'generated {len(KEYS)} icons, {W}x{H}, rgba={len(buf)} bytes -> {out}')
