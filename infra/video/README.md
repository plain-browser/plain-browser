# Video production (YouTube video about building Plain Browser)

Tooling that recorded the screen shots and assembled the rough cut for `docs/VIDEO_PLAN.md`. Everything runs on
the recording VM DW-VIDEO (`plainbrowser-video` on the Unraid host, see `infra/unraid-vm/README.md`); the scripts in
`host/` drive it over SSH from a Linux machine.
The addresses are not in the repository: set `PB_VIDEO_VM=user@host` (the recording VM) and `PB_UNRAID=user@host` (the Unraid
server, for `shot.sh` and `gexec.sh`) in the shell that runs the scripts.

## Layout
- `shots/shotlib.ps1`: helpers used by every shot: OBS over its WebSocket (`obs-ws.ps1`, localhost:4455, no auth,
  inbound blocked by a firewall rule), scene switching, a browser started maximized with its own profile
  (`C:\PlainBrowser\profile-video`), typing into the address bar, eased mouse moves and clicks, controls found by
  UI Automation name, a Windows Terminal with a short prompt (`termprofile.ps1`).
- `shots/<segment>-<name>.ps1`: one shot each; `run.ps1` runs the one named in `current.txt` and is started by the
  scheduled task `PBShot` (interactive session, so windows and keyboard input reach the desktop).
- `web/`: demo pages served by `python -m http.server 80 --bind 127.0.0.1` (task `PBWeb`) as `http://kurier.test/`
  (hosts file entry). `news.html` loads real ad and tracker URLs; `build.html` replays the build VM's real
  `.ninja_log` (`ninja.json`, generated from it, not committed); `diagram.html` animates `docs/video-assets/*.svg`;
  `card.html` renders placeholder cards for the to-camera parts.
- `assemble.py`: rough cut. Reads the narration from `docs/VIDEO_SCRIPT_*.md`, renders the to-camera text as cards
  (headless Plain Browser), lays the voice-over text as subtitles over the matching shots (freezing the last frame
  while the text is longer than the shot), and writes `montaz/plain-browser-montaz.mp4` with chapters, a `.srt` and
  `rozdzialy.txt` (YouTube chapter list).
- `host/`: `up.sh` (copy shot scripts with a UTF-8 BOM, which Windows PowerShell 5 needs for Polish text),
  `runshot.sh <shot>`, `shot.sh` (console screenshot via virsh), `frames.sh` (frames of a recording),
  `gexec.sh` (command through the QEMU guest agent when SSH is down), `montage.py`.

## Things that bit us
- `localhost`, `127.0.0.1` and private ranges are `$generichide` in EasyList, so element hiding does not show there;
  hence the `kurier.test` host name.
- Every start of a new browser profile makes one failed Windows logon (Chromium's blank-password check of the
  password manager). Ten in ten minutes lock the account with Windows 11's default lockout policy, and then SSH
  fails with "Connection reset". Reuse the profile; unlock with `gexec.sh` and `[ADSI]"WinNT://./builder,user"`.
- OBS on the VM renders with the Microsoft Basic Render Driver. Window capture of a fullscreen (F11) window and the
  terminal's default capture method gave white or black frames: use `--app=` windows instead of F11, method 2
  (Windows Graphics Capture) for the terminal, and set `monitor_id` on the display capture source.
- Windows Terminal splits its command line at `;`; start PowerShell with `-File`.
- `chrome://` URLs passed on the command line open an empty tab; type them into the address bar.
- The shields bubble is not visible to UI Automation as a window; it is clicked by position (toggle at 1808,171 on
  a maximized 1920x1080 window).

## Segment 1 with the author (`avatar/`)
- `parts.txt`: the narration of segment 1 in three parts (A voice-over on the hook shot, B to camera, C closing).
- `tts.ps1`: placeholder voice from the Windows OneCore voice "Adam" (pl-PL), one WAV per part, so the timing
  can be checked before the author records the real voice. Replace `A.wav`, `B.wav`, `C.wav` with the recording
  and run `compose.py` again; the subtitles and part lengths follow the audio.
- `compose.py`: the author's studio portrait (`dist/video/postac/IMG_3373.PNG`, copied as `portret.jpg`) in a
  circle in the corner of the screen shots during the voice-over, and large in a studio frame (`web/studio.html`,
  name bar) with a slow push-in and a waveform during the to-camera part; burned-in subtitles; title card.
  No generated lip movement: a moving mouth needs the real camera recording.
- This ffmpeg build draws `showwaves` colours wrongly; the waveform is drawn white and tinted with `colorchannelmixer`.
