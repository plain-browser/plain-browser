import type { Metadata } from "next";
import { Doc } from "@/components/Doc";

export const metadata: Metadata = {
  title: "Polityka prywatności",
  description: "Plain Browser niczego nie zbiera: bez telemetrii, bez konta, bez serwera odbierającego dane z przeglądarki.",
  alternates: { canonical: "/pl/privacy/", languages: { en: "/privacy/", pl: "/pl/privacy/" } },
};

export default function Page() {
  return <Doc lang="pl" file="docs/PRIVACY.pl.md" />;
}
