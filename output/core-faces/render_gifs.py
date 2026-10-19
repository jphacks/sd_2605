"""Render Core face previews from faces.h geometry. Requires Pillow.
Host rendering preview; does not verify device animation or pixel-identical rasterization.
Run: python output/core-faces/render_gifs.py
"""
from PIL import Image, ImageDraw
from pathlib import Path
import math
OUT=Path(__file__).resolve().parent

def rgb(r,g,b):
    v=((r&248)<<8)|((g&252)<<3)|(b>>3)
    return (((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31)
EYE=rgb(52,38,44); GLASS=rgb(58,50,62)
COLORS={'pink':(rgb(255,205,225),rgb(244,130,165)), 'yellow':(rgb(255,232,150),rgb(248,172,120)), 'blue':(rgb(150,200,255),rgb(250,138,168))}

def render(name,mood='idle',blink=0):
    fill,blush=COLORS[name]
    im=Image.new('RGB',(320,240),fill); d=ImageDraw.Draw(im)
    def ell(x,y,rx,ry,c): d.ellipse((x-rx,y-ry,x+rx,y+ry),fill=c)
    def stroke(x0,y0,x1,y1,r,c=EYE):
        n=max(abs(x1-x0),abs(y1-y0))
        for i in range(n+1): ell(x0+int((x1-x0)*i/max(n,1)),y0+int((y1-y0)*i/max(n,1)),r,r,c)
    def path(points,r,c=EYE):
        for a,b in zip(points,points[1:]): stroke(*a,*b,r,c)
    def lashes(x,closed=False):
        for a in ([204,220,236] if closed else [202,226,250]):
            a=math.radians(a if x==86 else 180-a); ca=math.cos(a); sa=math.sin(a)
            if closed:
                bx=x+int(42*ca); by=96+int(42*.7*sa); s=1 if x==86 else -1
                stroke(bx,by,bx+int(s*sa*15),by+int(s*(-.7)*ca*15),2)
            else:
                px=int(-sa*5); py=int(ca*5); rx=x+int(37*ca); ry=96+int(37*sa)
                d.polygon([(rx+px,ry+py),(rx-px,ry-py),(x+int(52*ca),96+int(52*sa))],fill=EYE)
    for x in [86,234]:
        if name=='pink':
            if mood=='sleep' or (mood=='fun' and x==86):
                path([(x+int(42*math.cos(math.radians(a))),96+int(42*.7*math.sin(math.radians(a)))) for a in range(196,345,6)],2)
                lashes(x,True)
            else:
                ry=42-39*blink//100; ell(x,96,42,ry,EYE)
                if blink<60:
                    ell(x+10,96-ry//5,16,max(3,ry*2//5),'white'); ell(x-14,96+ry//4,7,7,'white')
                lashes(x,blink>=60)
        elif name=='yellow':
            if mood=='fun':
                s=1 if x==86 else -1; path([(x-s*30,76),(x+s*20,96),(x-s*30,116)],4)
            else:
                ry=42-40*blink//100; ell(x,96,42,ry,EYE)
                if blink<60:
                    ell(x+7,96-ry*3//4,31,ry//2,fill); ell(x-14,96+ry//6,3,3,'white'); ell(x-5,96+ry//2,2,2,'white')
                    path([(x-44+int(88*i/10),49-int(9*(1-(2*i/10-1)**2)+.5)) for i in range(11)],3)
        else:
            ry=42-40*blink//100
            if mood=='sing':
                for delta in [0,1]:
                    d.line([(x-42,101+delta),(x,92+delta),(x+42,101+delta)],fill=EYE)
            elif mood=='wake':
                ell(x,96,46,ry+4,'white'); ell(x,96,41,max(2,ry-1),EYE)
                if blink<45: ell(x-3,96-ry//3,2,2,'white')
            elif mood=='sleep': ell(x,96,42,2,EYE)
            elif mood=='laugh':
                ell(x,96,42,42,EYE); d.rectangle((x-43,53,x+42,95),fill=fill); ell(x-14,106,8,8,'white')
            else:
                ell(x,96,42,ry,EYE)
                if blink<45: ell(x-14,96-ry//3,14,max(1,ry//3),'white')
    if name=='blue':
        for x in [86,234]:
            for r in [51,50,49]: d.ellipse((x-r,96-r,x+r,96+r),outline=GLASS)
        for delta in [-1,0,1]: d.line((135,96+delta,185,96+delta),fill=GLASS)
        d.line((37,96,8,90),fill=GLASS); d.line((283,96,312,90),fill=GLASS)
    for x in [44,276]: ell(x,160,30,14,blush)
    if mood in ['idle','fun']:
        if name=='pink': path([(136,161),(148,179),(160,164),(172,179),(184,161)],3,'black')
        elif name=='yellow': path([(148+int(24*i/12+.5),166+int(9*(1-(2*i/12-1)**2)+.5)) for i in range(13)],3,'black')
        else:
            d.polygon([(136,157),(184,157),(175,178),(145,178)],fill='black'); ell(160,178,15,7,'black'); ell(160,177,17,10,rgb(246,132,146))
    elif mood=='sing': ell(160,170,22,26,'black')
    elif mood=='wake': ell(160,170,12,12,'black')
    elif mood=='laugh':
        ell(160,170,24,28,'black'); ell(160,185,13,9,rgb(246,132,146))
    else:
        ell(160,170,13,9,'black'); ell(160,161,17,9,fill)
    if mood=='sing':
        for x,y in [(258,72),(294,46)]:
            ell(x,y,4,4,'cyan'); d.line([(x+4,y),(x+4,y-16),(x+12,y-11)],fill='cyan')
    return im

# Close and reopen eyes smoothly, then cycle through firmware expressions.
timeline=[('idle',0,1500)]
for b in [25,50,75,100,75,50,25,0]: timeline.append(('idle',b,60))
timeline += [('idle',0,900),('wake',0,700),('idle',0,350),('sing',0,1000),('laugh',0,800),('fun',0,1100),('sleep',0,900),('idle',0,700)]
frames={name:[render(name,mood,b) for mood,b,_ in timeline] for name in COLORS}
durations=[ms for _,_,ms in timeline]
for name,seq in frames.items():
    seq[0].save(OUT/f'core-face-{name}.gif',save_all=True,append_images=seq[1:],duration=durations,loop=0,disposal=2,optimize=False)
combined=[]
for i in range(len(timeline)):
    im=Image.new('RGB',(960,240))
    for j,name in enumerate(COLORS): im.paste(frames[name][i],(j*320,0))
    combined.append(im)
combined[0].save(OUT/'core-faces.gif',save_all=True,append_images=combined[1:],duration=durations,loop=0,disposal=2,optimize=False)
for p in OUT.glob('*.gif'):
    with Image.open(p) as im:
        duration=0
        for i in range(im.n_frames): im.seek(i); duration+=im.info['duration']
        assert im.info.get('loop')==0
        print(p.name,im.size,im.n_frames,'frames',duration,'ms',p.stat().st_size,'bytes')
