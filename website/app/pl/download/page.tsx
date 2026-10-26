import type { Metadata } from "next";
import { Download } from "@/components/Download";

export const metadata: Metadata = {
  title: "Pobierz",
  description: "Pobierz Plain Browser dla Windows 11 x64: instalator, wersja przenośna ZIP i sumy kontrolne.",
  alternates: { canonical: "/pl/download/", languages: { en: "/download/", pl: "/pl/download/" } },
};

export default async function Page() {
  return <Download lang="pl" />;
}
