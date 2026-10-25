"""Real-site page load and main-thread stall test (run on the Windows build VM; needs `pip install websocket-client`).
Loads each site in headless Plain Browser over the remote-debugging protocol, with the adblock filter on and
off, and reports load time, blocked requests and long tasks (main-thread stalls over 200 ms, from PerformanceObserver).
Usage: python scripts\\perf-sites.py <chrome.exe> [runs]"""
import json, os, shutil, subprocess, sys, tempfile, time, urllib.request
import websocket

SITES = ["https://www.wp.pl", "https://www.onet.pl", "https://www.interia.pl", "https://www.cnn.com",
         "https://www.bbc.com", "https://www.theguardian.com", "https://www.dailymail.co.uk",
         "https://www.reddit.com", "https://pl.wikipedia.org/wiki/Polska", "https://www.nytimes.com"]
HOOK = """
window.__lt = []; try { new PerformanceObserver(l => { for (const e of l.getEntries()) window.__lt.push(Math.round(e.duration)); })
  .observe({entryTypes: ['longtask']}); } catch (e) {}
"""


class Tab:
    def __init__(self, port):
        tabs = json.load(urllib.request.urlopen("http://127.0.0.1:%d/json/list" % port))
        page = [t for t in tabs if t["type"] == "page"][0]
        self.ws = websocket.create_connection(page["webSocketDebuggerUrl"], timeout=60, suppress_origin=True)
        self.n = 0
        self.blocked = 0

    def call(self, method, **params):
        self.n += 1
        mid = self.n
        self.ws.send(json.dumps({"id": mid, "method": method, "params": params}))
        while True:
            m = json.loads(self.ws.recv())
            if m.get("method") == "Network.loadingFailed" and m["params"].get("blockedReason") == "inspector":
                pass
            if m.get("method") == "Network.loadingFailed" and "BLOCKED_BY_CLIENT" in m["params"].get("errorText", ""):
                self.blocked += 1
            if m.get("id") == mid:
                return m.get("result", {})

    def wait_load(self, timeout):
        end = time.time() + timeout
        self.ws.settimeout(2)
        while time.time() < end:
            try:
                m = json.loads(self.ws.recv())
            except Exception:
                continue
            if m.get("method") == "Network.loadingFailed" and "BLOCKED_BY_CLIENT" in m["params"].get("errorText", ""):
                self.blocked += 1
            if m.get("method") == "Page.loadEventFired":
                return True
        return False


def run(chrome, extra, tag, port, runs):
    ud = tempfile.mkdtemp(prefix="perf_")
    p = subprocess.Popen([chrome, "--headless=new", "--disable-gpu", "--user-data-dir=" + ud,
                          "--remote-debugging-port=%d" % port, "--no-first-run"] + extra + ["about:blank"],
                         stderr=subprocess.DEVNULL)
    time.sleep(4)
    rows = []
    try:
        t = Tab(port)
        t.call("Page.enable"); t.call("Network.enable")
        t.call("Page.addScriptToEvaluateOnNewDocument", source=HOOK)
        for site in SITES:
            for r in range(runs):
                t.blocked = 0
                t0 = time.time()
                t.call("Page.navigate", url=site)
                ok = t.wait_load(40)
                load = time.time() - t0
                time.sleep(3)  # let late scripts and ads run
                v = t.call("Runtime.evaluate", expression="JSON.stringify(window.__lt||[])", returnByValue=True)
                lt = json.loads(v.get("result", {}).get("value", "[]"))
                rows.append((site.split("//")[1][:28], r, ok, load, t.blocked, len(lt), max(lt) if lt else 0, sum(lt)))
    finally:
        p.kill()
        shutil.rmtree(ud, ignore_errors=True)
    print("== %s" % tag)
    print("%-29s %3s %5s %7s %8s %6s %8s %8s" % ("site", "run", "load", "sec", "blocked", "long", "max_ms", "sum_ms"))
    for s, r, ok, load, bl, n, mx, sm in rows:
        print("%-29s %3d %5s %7.1f %8d %6d %8d %8d" % (s, r, "ok" if ok else "TMO", load, bl, n, mx, sm))
    sys.stdout.flush()


if __name__ == "__main__":
    chrome = sys.argv[1]
    runs = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    run(chrome, [], "adblock ON", 9371, runs)
    run(chrome, ["--disable-features=DraftwayAdblock"], "adblock OFF", 9372, runs)
    print("DONE")
