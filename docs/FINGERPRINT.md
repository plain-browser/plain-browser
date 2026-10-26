# Fingerprint protection

Status 2026-10-26: written against the Chromium 156 headers, JS logic unit-tested in Node, **not compiled or run in
the browser yet**.

## What it does
`src/plainbrowser/fingerprint` injects `renderer/fingerprint_script.js` into the main world of every document, from
`FingerprintAgent::DidCreateScriptContext`, before any page script runs. The script wraps a few APIs with
deterministic noise:

| Surface | Change |
|---|---|
| 2D canvas readback (`getImageData`, `toDataURL`, `toBlob`, `convertToBlob`) | low bit of one channel flipped in about 3 % of pixels; same input gives the same output within a session |
| WebGL `UNMASKED_VENDOR_WEBGL` / `UNMASKED_RENDERER_WEBGL` | one common Intel/ANGLE string for everyone (bucketing, not noise) |
| Web Audio (`getChannelData`, `copyFromChannel`, `AnalyserNode.get*Data`) | values scaled by a per-site factor in [0.99, 1) |
| `navigator.hardwareConcurrency`, `navigator.deviceMemory` | rounded down to 2, 4 or 8 |

The seed is `FastHash(session_key + "|" + top-level site)`. The browser process draws the 128-bit session key once
per run and passes it to every renderer on the command line (`--plainbrowser-fingerprint-session-key`,
`patches/80-fingerprint/0002`), so one site sees the same values in every tab for the whole session, two sites see
different values, and a restart changes everything. The site is the eTLD+1 of the top-level frame, so an embedded
tracker frame sees the noise of the page it is on, not its own.

Wrappers report `[native code]` from `Function.prototype.toString` and keep the original `name` and `length`.

## Exceptions (banks)
Banks fingerprint the device for fraud detection; a fingerprint that changes every session triggers extra
verification or blocks the login. Sites on the exception list get no script at all:
- built-in list in `renderer/fingerprint_exceptions.cc` (Polish banks, BLIK, Przelewy24, PayU, PayPal, Revolut, Wise);
- `--fingerprint-exceptions=mbank.pl,example.com` (eTLD+1, comma-separated) is forwarded to renderers.

Kill switch: `--disable-features=PlainBrowserFingerprintProtection`.

## Limits
- Page-world JavaScript overrides are easier to detect than Blink-level changes (Brave's approach). A determined
  script can snapshot a pristine iframe early or compare timing. This slice buys protection against the common
  fingerprinting libraries at near-zero rebase cost; Blink patches for canvas and audio are the next step if needed.
- Not covered: workers (no document), WebGL `readPixels`, fonts, screen size, `navigator.plugins`.
- No UI for the exception list yet; the switch is the only user control.

## Wiring
- `patches/80-fingerprint/0001-renderer-agent.patch`: `FingerprintAgent::Create` in
  `ChromeContentRendererClient::RenderFrameCreated` (next to the adblock cosmetic agent).
- `patches/80-fingerprint/0002-browser-renderer-switches.patch`: `AppendRendererCommandLineSwitches` in
  `ChromeContentBrowserClient::AppendExtraCommandLineSwitches`.
- `patches/00-build/0001-gn-deps.patch`: `//plainbrowser:browser` and `//plainbrowser:renderer` groups.
- `renderer/embed_text.py` turns the `.js` into a C++ constant at build time (GN action).

## Test
```bash
node src/plainbrowser/fingerprint/renderer/fingerprint_script_test.js src/plainbrowser/fingerprint/renderer/fingerprint_script.js
```
The Node harness (stubbed `navigator`, canvas, audio and WebGL) checks determinism per seed, difference between
seeds, that only low bits change in few pixels, native-looking `toString`, and single application of the audio
factor. In the browser: open `https://browserleaks.com/canvas` in two tabs (same hash) and after a restart
(different hash); open a bank from the list (hash equals a stock ungoogled-chromium).
