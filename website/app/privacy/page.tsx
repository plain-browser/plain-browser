import type { Metadata } from "next";
import { Doc } from "@/components/Doc";

export const metadata: Metadata = {
  title: "Privacy policy",
  description: "Plain Browser collects nothing: no telemetry, no account, no server that receives browser data.",
  alternates: { canonical: "/privacy/", languages: { en: "/privacy/", pl: "/pl/privacy/" } },
};

export default function Page() {
  return <Doc lang="en" file="docs/PRIVACY.md" />;
}
