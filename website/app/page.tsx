import type { Metadata } from "next";
import { Home } from "@/components/Home";
import { dict } from "@/lib/i18n";

export const metadata: Metadata = {
  description: dict.en.metaDescription,
  alternates: { canonical: "/", languages: { en: "/", pl: "/pl/" } },
};

export default async function Page() {
  return <Home lang="en" />;
}
