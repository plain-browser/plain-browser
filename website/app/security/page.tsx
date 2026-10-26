import type { Metadata } from "next";
import { Doc } from "@/components/Doc";

export const metadata: Metadata = {
  title: "Security policy",
  description: "How to report a vulnerability in Plain Browser and how to verify a release.",
  alternates: { canonical: "/security/", languages: { en: "/security/", pl: "/pl/security/" } },
};

export default function Page() {
  return <Doc lang="en" file="SECURITY.md" />;
}
