import https from "node:https";

// Build-time GET through node:https rather than fetch: Next.js patches fetch and, in a static export,
// keeps every answer in .next/cache for a year (cache: "no-store" is refused there), so a once-cached
// GitHub or Store answer would be shown by every later build.
export function getText(url: string, headers: Record<string, string> = {}, timeoutMs = 15000): Promise<string> {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { headers }, (res) => {
      if (res.statusCode !== 200) {
        res.resume();
        reject(new Error(`HTTP ${res.statusCode}`));
        return;
      }
      res.setEncoding("utf8");
      let body = "";
      res.on("data", (chunk: string) => (body += chunk));
      res.on("end", () => resolve(body));
      res.on("error", reject);
    });
    req.setTimeout(timeoutMs, () => req.destroy(new Error("timeout")));
    req.on("error", reject);
  });
}
