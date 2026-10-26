import type { Metadata } from "next";
import { Doc } from "@/components/Doc";

export const metadata: Metadata = {
  title: "Polityka bezpieczeństwa",
  description: "Jak zgłosić podatność w Plain Browser i jak zweryfikować wydanie.",
  alternates: { canonical: "/pl/security/", languages: { en: "/security/", pl: "/pl/security/" } },
};

export default function Page() {
  return <Doc lang="pl" file="docs/SECURITY.pl.md" />;
}
