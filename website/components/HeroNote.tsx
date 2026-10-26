"use client";
import type { Release } from "@/lib/release";
import { useRelease } from "./useRelease";

export function HeroNote({ initial, note, chromium }: { initial: Release; note: string; chromium: string }) {
  const r = useRelease(initial);
  return <p className="hero-note">{note} · v{r.version} · Chromium {chromium}</p>;
}
