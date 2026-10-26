import type { Metadata } from "next";
import { Home } from "@/components/Home";
import { dict } from "@/lib/i18n";

export const metadata: Metadata = {
  description: dict.pl.metaDescription,
  alternates: { canonical: "/pl/", languages: { en: "/", pl: "/pl/" } },
};

export default async function Page() {
  return <Home lang="pl" />;
}
