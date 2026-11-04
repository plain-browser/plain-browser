"use client";
import { useEffect, useState } from "react";

// After hydration, ask our own origin (the Worker in website/worker, cached at the edge) which version the
// Microsoft Store serves now, so the number follows the Store without a rebuild. Keeps the build-time value
// when the Worker is not there or does not know.
export function useStoreVersion(initial: string | null): string | null {
  const [version, setVersion] = useState(initial);
  useEffect(() => {
    const ctrl = new AbortController();
    fetch("/api/store-version", { signal: ctrl.signal, headers: { Accept: "application/json" } })
      .then((r) => (r.ok ? r.json() : null))
      .then((r: { version?: unknown } | null) => {
        if (r && typeof r.version === "string" && /^\d+(\.\d+){2,3}$/.test(r.version)) setVersion(r.version);
      })
      .catch(() => {});
    return () => ctrl.abort();
  }, []);
  return version;
}
