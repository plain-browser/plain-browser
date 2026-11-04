"use client";
import { useStoreVersion } from "./useStoreVersion";

export function HeroNote({ store, note, chromium }: { store: string | null; note: string; chromium: string }) {
  const v = useStoreVersion(store);
  return <p className="hero-note">{note}{v ? ` · v${v}` : ""} · Chromium {chromium}</p>;
}
