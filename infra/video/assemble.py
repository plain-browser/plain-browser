"""Rough cut of the Plain Browser video from the narration scripts and the recorded shots (runs on DW-VIDEO)."""
import glob, json, os, re, subprocess, sys, urllib.parse
ROOT = r'C:\PlainBrowser\asm'; VID = r'C:\Users\builder\Videos'; OUT = os.path.join(VID, 'montaz')
FF = glob.glob(os.path.expandvars(r'%LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*\*\bin'))[0]
FFMPEG, FFPROBE = os.path.join(FF, 'ffmpeg.exe'), os.path.join(FF, 'ffprobe.exe')
CHROME = r'C:\PlainBrowser\PlainBrowser_0.1.3-alpha.1_x64\chrome.exe'
WPS = 2.5  # 150 words per minute
TITLES = {1: 'Hook', 2: 'Po co własna przeglądarka', 3: 'Silnik od zera? Nie', 4: 'Patche zamiast forka', 5: '7 godzin kompilacji',
          6: 'Branding', 7: 'Adblock w środku przeglądarki', 8: 'MV2, fingerprint, utm_', 9: 'Testy i rebase co 4 tygodnie',
          10: 'MSIX i Microsoft Store', 11: 'Co nie wyszło', 12: 'Outro'}
CLIPS = {1: ['seg01-hook'], 2: [], 3: ['diag01-layers', 'seg03-repo'], 4: ['seg04-patches', 'seg04-check'],
         5: ['seg05-timelapse', 'seg05-rebuild', 'diag03-rebuild', 'seg05-compare'], 6: ['seg06-branding'],
         7: ['seg07-shields', 'diag02-request', 'seg07-taskmgr', 'seg07-pages'], 8: ['seg08-mv2', 'seg08-utm', 'seg08-fp-1', 'seg08-fp-2'],
         9: ['seg09-tests', 'diag04-rebase'], 10: ['seg10-msix', 'seg10-site'], 11: ['seg11-git'], 12: ['seg12-outro']}

def blocks():
    segs = {}
    for f in ['VIDEO_SCRIPT_1_5.md', 'VIDEO_SCRIPT_REST.md', 'VIDEO_SCRIPT_7.md']:
        seg = None; kind = None; buf = []
        def flush():
            if seg and kind and buf: segs.setdefault(seg, []).append((kind, ' '.join(' '.join(buf).split())))
        for line in open(os.path.join(ROOT, f), encoding='utf-8'):
            m = re.match(r'##\s+Segment\s+(\d+)', line) or re.match(r'#\s+Scenariusz narracji: segment (\d+)\.', line)
            if m: flush(); seg = int(m.group(1)); kind = None; buf = []; continue
            if line.startswith('## ') or line.startswith('---'): flush(); kind = None; buf = []; continue
            m = re.match(r'\*\*\[(DO KAMERY|OFF)\]\*\*\s*(.*)', line)
            if m: flush(); kind = m.group(1); buf = [m.group(2)] if m.group(2).strip() else []; continue
            if line.startswith('**['): flush(); kind = None; buf = []; continue
            if kind and line.strip(): buf.append(line.strip())
        flush()
    return segs

def chunks(text, n=70):
    w = text.split(); out = []; cur = []
    for s in re.split(r'(?<=[.!?])\s+', text):
        sw = s.split()
        if cur and len(cur) + len(sw) > n: out.append(' '.join(cur)); cur = []
        cur += sw
    if cur: out.append(' '.join(cur))
    return out

def dur(p): return float(subprocess.check_output([FFPROBE, '-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', p]).decode().strip())

def card(path, k, s, t):
    q = urllib.parse.urlencode({'k': k, 's': s, 't': t})
    subprocess.run([CHROME, '--headless', '--disable-gpu', '--hide-scrollbars', '--user-data-dir=' + os.path.join(ROOT, 'hp'),
                    '--window-size=1920,1080', '--screenshot=' + path, 'http://kurier.test/card.html?' + q], capture_output=True, timeout=60)

def enc(args, out, pre=''):
    if os.path.exists(out): return
    base = [FFMPEG, '-v', 'error', '-y']
    tail = ['-vf', pre + 'scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,fps=30,format=yuv420p',
            '-an', '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '20', out]
    subprocess.run(base + args + tail, check=True)

def ts(t): h = int(t // 3600); m = int(t % 3600 // 60); s = t % 60; return f'{h:02}:{m:02}:{s:06.3f}'.replace('.', ',')

def main():
    os.makedirs(OUT, exist_ok=True); os.makedirs(os.path.join(ROOT, 'parts'), exist_ok=True)
    segs = blocks(); parts = []; subs = []; chapters = []; t = 0.0; report = []
    for n in range(1, 13):
        items = []; clips = [c for c in CLIPS[n] if os.path.exists(os.path.join(VID, c + '.mkv'))]; ci = 0; pending = []
        def put_clip(texts):
            nonlocal ci
            if ci < len(clips): items.append(('clip', clips[ci], texts)); ci += 1
            else: items.append(('offcard', None, texts))
        for kind, text in segs.get(n, []):
            if kind == 'OFF': pending.append(text)
            else:
                if pending: put_clip(pending); pending = []
                for c in chunks(text): items.append(('card', c, None))
        if pending: put_clip(pending)
        while ci < len(clips): items.append(('clip', clips[ci], [])); ci += 1
        chapters.append((t, f'{n}. {TITLES[n]}')); seg_start = t
        for i, (kind, a, texts) in enumerate(items):
            out = os.path.join(ROOT, 'parts', f'{n:02}-{i:02}.mp4')
            if kind == 'card' or kind == 'offcard':
                lines = chunks(' '.join(texts)) if kind == 'offcard' else [a]
                for j, line in enumerate(lines):
                    png = os.path.join(ROOT, 'parts', f'{n:02}-{i:02}-{j}.png'); o2 = out.replace('.mp4', f'-{j}.mp4')
                    d = max(4.0, len(line.split()) / WPS)
                    card(png, 'DO KAMERY' if kind == 'card' else 'NARRACJA Z OFFU', f'{n}. {TITLES[n]}', line)
                    enc(['-loop', '1', '-framerate', '30', '-i', png, '-t', f'{d:.2f}'], o2)
                    parts.append(o2); t += d
            else:
                src = os.path.join(VID, a + '.mkv'); cd = dur(src); words = sum(len(x.split()) for x in texts)
                need = words / WPS; d = max(cd, need)
                enc(['-i', src, '-t', f'{d:.2f}'], out, pre=f'tpad=stop_mode=clone:stop_duration={max(0, d - cd) + 1:.2f},')
                if texts:
                    sents = [s for x in texts for s in chunks(x, 16)]; tw = sum(len(s.split()) for s in sents); u = t
                    for s in sents:
                        sd = d * len(s.split()) / tw; subs.append((u, u + sd, s)); u += sd
                parts.append(out); t += d
        report.append(f'{n:2}. {TITLES[n]:32} {t - seg_start:7.1f} s  ({len(items)} pozycji)')
    with open(os.path.join(ROOT, 'list.txt'), 'w', encoding='utf-8') as f:
        for p in parts: f.write("file '" + p.replace('\\', '/') + "'\n")
    with open(os.path.join(OUT, 'plain-browser-montaz.srt'), 'w', encoding='utf-8') as f:
        for i, (a, b, s) in enumerate(subs, 1): f.write(f'{i}\n{ts(a)} --> {ts(b)}\n{s}\n\n')
    with open(os.path.join(ROOT, 'meta.txt'), 'w', encoding='utf-8') as f:
        f.write(';FFMETADATA1\ntitle=Zbudowałem własną przeglądarkę (montaż roboczy)\n')
        for k, (a, name) in enumerate(chapters):
            b = chapters[k + 1][0] if k + 1 < len(chapters) else t
            f.write(f'[CHAPTER]\nTIMEBASE=1/1000\nSTART={int(a * 1000)}\nEND={int(b * 1000)}\ntitle={name}\n')
    with open(os.path.join(OUT, 'rozdzialy.txt'), 'w', encoding='utf-8') as f:
        for a, name in chapters: f.write(f'{int(a // 60)}:{int(a % 60):02} {name}\n')
    final = os.path.join(OUT, 'plain-browser-montaz.mp4')
    subprocess.run([FFMPEG, '-v', 'error', '-y', '-f', 'concat', '-safe', '0', '-i', os.path.join(ROOT, 'list.txt'),
                    '-f', 'lavfi', '-i', 'anullsrc=r=48000:cl=stereo', '-i', os.path.join(OUT, 'plain-browser-montaz.srt'),
                    '-i', os.path.join(ROOT, 'meta.txt'), '-map', '0:v', '-map', '1:a', '-map', '2:s', '-map_metadata', '3', '-map_chapters', '3',
                    '-c:v', 'copy', '-c:a', 'aac', '-b:a', '96k', '-c:s', 'mov_text', '-metadata:s:s:0', 'language=pol',
                    '-shortest', '-movflags', '+faststart', final], check=True)
    report.append(f'RAZEM {t:.1f} s = {int(t // 60)}:{int(t % 60):02}'); report.append(final)
    open(os.path.join(OUT, 'raport.txt'), 'w', encoding='utf-8').write('\n'.join(report)); print('\n'.join(report))

if __name__ == '__main__': main()
