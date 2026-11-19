"""Segment 1 with the author's real photo (no generated lip movement): PiP circle over the screen shots while the
voice-over runs, a studio shot with the portrait for the to-camera part, burned-in subtitles. Runs on DW-VIDEO."""
import glob, json, os, re, subprocess, urllib.parse
D = r'C:\PlainBrowser\avatar'; V = r'C:\Users\builder\Videos'; W = os.path.join(D, 'work'); os.makedirs(W, exist_ok=True)
FF = glob.glob(os.path.expandvars(r'%LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*\*\bin'))[0]
FFMPEG, FFPROBE = os.path.join(FF, 'ffmpeg.exe'), os.path.join(FF, 'ffprobe.exe')
CHROME = r'C:\PlainBrowser\PlainBrowser_0.1.3-alpha.1_x64\chrome.exe'
NAME, ROLE = 'Tomasz Lasota', 'autor Plain Browser'
ENC = ['-c:v', 'libx264', '-preset', 'veryfast', '-crf', '19', '-pix_fmt', 'yuv420p', '-r', '30', '-c:a', 'aac', '-b:a', '160k', '-ar', '48000', '-ac', '2']
def ff(*a): subprocess.run([FFMPEG, '-v', 'error', '-y', *a], check=True, cwd=W)
def dur(p): return float(subprocess.check_output([FFPROBE, '-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', p]).decode())
def shot(url, out):
    subprocess.run([CHROME, '--headless', '--disable-gpu', '--hide-scrollbars', '--user-data-dir=' + os.path.join(W, 'hp'),
                    '--window-size=1920,1080', '--screenshot=' + os.path.join(W, out), url], capture_output=True, timeout=60)
def ts(t): return f'{int(t // 3600):02}:{int(t % 3600 // 60):02}:{t % 60:06.3f}'.replace('.', ',')
def srt(name, text, total, start=0.25):
    sents = [s for s in re.split(r'(?<=[.!?:])\s+', text) if s]
    chunks = []
    for s in sents:
        w = s.split()
        while len(w) > 12: chunks.append(' '.join(w[:10])); w = w[10:]
        chunks.append(' '.join(w))
    n = sum(len(c) for c in chunks); t = start; span = total - start - 0.3; out = []
    for i, c in enumerate(chunks, 1):
        d = span * len(c) / n; out.append(f'{i}\n{ts(t)} --> {ts(t + d)}\n{c}\n'); t += d
    open(os.path.join(W, name), 'w', encoding='utf-8').write('\n'.join(out))
SUB = "subtitles={}:fontsdir=C\\\\:/Windows/Fonts:force_style='FontName=Segoe UI Semibold,FontSize=17,PrimaryColour=&H00FFFFFF,OutlineColour=&H00101010,BorderStyle=3,Outline=6,Shadow=0,MarginV=34'"
texts = dict(l.split('|', 1) for l in open(os.path.join(D, 'parts.txt'), encoding='utf-8').read().strip().splitlines())
aud = {k: os.path.join(D, k + '.wav') for k in 'ABC'}; L = {k: dur(v) for k, v in aud.items()}
for k in 'ABC': srt(k + '.srt', texts[k], L[k])
# stills: studio background with the name bar, closing title
shot('http://kurier.test/studio.html?' + urllib.parse.urlencode({'n': NAME, 'r': ROLE}), 'studio.png')
shot('http://kurier.test/studio.html', 'studio-empty.png')
P = os.path.join(D, 'portret.jpg')
# circle portrait 340 px with alpha and a blue ring
ff('-i', P, '-vf', "crop=1300:1300:10:200,scale=340:340,format=rgba,geq=r='r(X,Y)':g='g(X,Y)':b='b(X,Y)':a='if(lte(hypot(X-170,Y-170),166),255,0)'", '-frames:v', '1', 'circle.png')
ff('-f', 'lavfi', '-i', 'color=c=0x2563eb@1:s=356x356,format=rgba', '-vf', "geq=r=37:g=99:b=235:a='if(between(hypot(X-178,Y-178),166,178),255,0)'", '-frames:v', '1', 'ring.png')
# PiP layer filter: ring + circle bottom-right, small waveform under it
def pip(base, audio_idx):
    return f"[{audio_idx}:a]anullsink;{base}[ring]overlay=W-w-44:H-h-150[b1];[b1][circ]overlay=W-w-52:H-h-158"
# A: hook shot (voice-over) + PiP
ff('-i', os.path.join(V, 'seg01-hook.mkv'), '-i', aud['A'], '-loop', '1', '-i', 'ring.png', '-loop', '1', '-i', 'circle.png', '-filter_complex',
   f"[0:v]scale=1920:1080,tpad=stop_mode=clone:stop_duration=30,trim=0:{L['A']:.2f},setpts=PTS-STARTPTS[v];[2:v]format=rgba[ring];[3:v]format=rgba[circ];"
   + pip('[v]', 1) + f",{SUB.format('A.srt')}[out]", '-map', '[out]', '-map', '1:a', '-t', f"{L['A']:.2f}", *ENC, 'A.mp4')
# B: studio, portrait left with a slow push-in, waveform right, subtitles
ff('-loop', '1', '-i', 'studio.png', '-loop', '1', '-i', P, '-i', aud['B'], '-filter_complex',
   f"[1:v]crop=1320:1700:0:100,scale=-2:1700,zoompan=z='min(1.0+on/{int(L['B']*30)}*0.08,1.08)':x='iw/2-(iw/zoom/2)':y='ih*0.18-(ih*0.18/zoom)':d=1:s=640x820:fps=30,"
   f"pad=652:832:6:6:color=0x2563eb[por];"
   f"[2:a]showwaves=s=760x220:mode=cline:colors=white:draw=full:scale=sqrt:rate=30,format=rgba,colorchannelmixer=rr=0.53:gg=0.81:bb=0.98:aa=2[wv];"
   f"[0:v][por]overlay=200:136[s1];[s1][wv]overlay=1020:420,{SUB.format('B.srt')}[out]",
   '-map', '[out]', '-map', '2:a', '-t', f"{L['B']:.2f}", *ENC, 'B.mp4')
# C: quick montage of four shots + PiP, then the title
cuts = [('seg05-timelapse.mkv', 20), ('seg07-shields.mkv', 14), ('seg08-mv2.mkv', 10), ('diag02-request.mkv', 12)]
seg = L['C'] / len(cuts); ins = []; fl = []
for i, (f, s) in enumerate(cuts):
    ins += ['-ss', str(s), '-t', f'{seg:.2f}', '-i', os.path.join(V, f)]
    fl.append(f"[{i}:v]scale=1920:1080,setsar=1,fps=30,setpts=PTS-STARTPTS[c{i}]")
n = len(cuts)
ff(*ins, '-i', aud['C'], '-loop', '1', '-i', 'ring.png', '-loop', '1', '-i', 'circle.png', '-filter_complex',
   ';'.join(fl) + ';' + ''.join(f'[c{i}]' for i in range(n)) + f"concat=n={n}:v=1:a=0[m];[{n+1}:v]format=rgba[ring];[{n+2}:v]format=rgba[circ];"
   + pip('[m]', n) + f",{SUB.format('C.srt')}[out]", '-map', '[out]', '-map', f'{n}:a', '-t', f"{L['C']:.2f}", *ENC, 'C.mp4')
ff('-loop', '1', '-i', 'studio-empty.png', '-f', 'lavfi', '-i', 'anullsrc=r=48000:cl=stereo', '-vf',
   "drawtext=fontfile=C\\\\:/Windows/Fonts/segoeuib.ttf:text='Zbudowałem własną przeglądarkę':fontsize=84:fontcolor=white:x=(w-tw)/2:y=(h-th)/2-30,"
   "drawtext=fontfile=C\\\\:/Windows/Fonts/segoeui.ttf:text='Plain Browser · Chromium bez Google':fontsize=36:fontcolor=0x9fb0c3:x=(w-tw)/2:y=(h/2)+60,fade=in:0:12",
   '-t', '3', *ENC, 'T.mp4')
open(os.path.join(W, 'list.txt'), 'w').write(''.join(f"file '{x}.mp4'\n" for x in 'ABCT'))
out = os.path.join(V, 'montaz', 'segment01-z-autorem.mp4')
ff('-f', 'concat', '-safe', '0', '-i', 'list.txt', '-c', 'copy', '-movflags', '+faststart', out)
print(out, round(dur(out), 2), 's')
