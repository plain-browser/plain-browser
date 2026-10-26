"use client";
import { useEffect, useState } from "react";
import type { Release } from "@/lib/release";

// After hydration, ask our own origin for the newest release. /api/latest-release is a Cloudflare
// Worker (website/worker) that caches GitHub's answer, so the visitor's browser never talks to GitHub
// and the static site shows a new version minutes after it is published, with no rebuild.
export function useRelease(initial: Release): Release {
  const [release, setRelease] = useState(initial);
  useEffect(() => {
    const ctrl = new AbortController();
    fetch("/api/latest-release", { signal: ctrl.signal, headers: { Accept: "application/json" } })
      .then((r) => (r.ok ? r.json() : null))
      .then((r: Release | null) => {
        if (r && typeof r.version === "string" && r.version && r.tag) setRelease(r);
      })
      .catch(() => {});
    return () => ctrl.abort();
  }, []);
  return release;
}
