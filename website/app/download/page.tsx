import type { Metadata } from "next";
import { Download } from "@/components/Download";

export const metadata: Metadata = {
  title: "Download",
  description: "Download Plain Browser for Windows 11 x64: installer, portable ZIP and checksums.",
  alternates: { canonical: "/download/", languages: { en: "/download/", pl: "/pl/download/" } },
};

export default async function Page() {
  return <Download lang="en" />;
}
