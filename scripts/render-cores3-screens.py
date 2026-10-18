from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/images'
S = 4

def rgb(r,g,b):
    return ((r >> 3)*255//31, (g >> 2)*255//63, (b >> 3)*255//31)

def canvas(color):
    im = Image.new('RGB', (320*S,240*S), color)
    return im, ImageDraw.Draw(im)

def ellipse(d,x,y,rx,ry,c,outline=False,width=1):
    box = ((x-rx)*S,(y-ry)*S,(x+rx)*S,(y+ry)*S)
    d.ellipse(box, outline=c, width=width*S) if outline else d.ellipse(box,fill=c)

def line(d,pts,c,width=1):
    d.line([(x*S,y*S) for x,y in pts],fill=c,width=width*S)

def rect(d,box,c,outline=False):
    box=tuple(v*S for v in box)
    d.rectangle(box,outline=c,width=S) if outline else d.rectangle(box,fill=c)

# Faces.h: BOY, MOOD_IDLE, blink=0.
im,d=canvas(rgb(150,200,255))
eye=rgb(52,38,44); glass=rgb(58,50,62)
for x in (86,234):
    ellipse(d,x,96,42,42,eye)
    ellipse(d,x-14,82,14,14,'white')
    for delta in range(3): ellipse(d,x,96,51-delta,51-delta,glass,True)
line(d,[(135,96),(185,96)],glass,3)
line(d,[(37,96),(8,90)],glass)
line(d,[(283,96),(312,90)],glass)
for x in (44,276): ellipse(d,x,160,30,14,rgb(250,138,168))
d.polygon([(136*S,157*S),(184*S,157*S),(175*S,178*S),(145*S,178*S)],fill='black')
ellipse(d,160,178,15,7,'black')
ellipse(d,160,177,17,10,rgb(246,132,146))
im.save(OUT/'cores3-face.png')

# CameraOnly.h: 320x240 debug layout. Telemetry and preview are illustrative.
im,d=canvas('black')
font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',8*S)
def text(x,y,s,c='white'):
    d.text((x*S,y*S),s,font=font,fill=c,stroke_width=0)
rect(d,(0,0,319,23),'#7b7d7b')
text(4,1,'DIRECTION MAP - tap to swap')
text(4,13,'L->R: LEAVE    R->L: ARRIVE')
rect(d,(0,28,159,147),'#252c30')
# No invented camera photo: explicitly labelled sample preview.
text(24,77,'CAMERA PREVIEW','#a0adb5')
text(43,91,'(sample)','#a0adb5')
rect(d,(0,28,159,147),'yellow',True)
for x in (53,107): line(d,[(x,28),(x,147)],'#7b7d7b')
rows=['COOLDOWN','Change:0.7 T:3%','FPS:20.0 LCD:19.0','Count:2 Dir:2','Last:LEFT_TO_RIGHT','x:127 dx:94','PIR:IDLE F:CONFIRMED']
for i,row in enumerate(rows): text(168,28+i*16,row)
text(168,140,'WiFi:ON API:200')
text(0,151,'MOTION DETECTED')
labels=['SENS +','SENS -','PAUSE/RUN','CHIME','TEST SOUND','RESET COUNT','VOL -','VOL +','WEB 50%']
for i,label in enumerate(labels):
    x=(i%3)*107; y=166+(i//3)*25
    rect(d,(x,y,x+105,y+23),'#7b7d7b',True)
    text(x+8,y+8,label)
im.save(OUT/'cores3-debug.png')
print('Created docs/images/cores3-face.png and cores3-debug.png (1280x960)')
