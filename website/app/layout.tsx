import type { Metadata } from "next";
import "./globals.css";
import { site } from "@/lib/site";

export const metadata: Metadata = {
  metadataBase: new URL(site.url),
  title: { default: site.name, template: `%s · ${site.name}` },
  description: "A plain Chromium browser for Windows: no Google services, built-in ad blocking, no tracking.",
  icons: { icon: [{ url: "/favicon.ico" }, { url: "/favicon-32.png", sizes: "32x32" }], apple: "/icon-192.png" },
  openGraph: { siteName: site.name, type: "website", images: ["/og.png"] },
  twitter: { card: "summary" },
  robots: { index: true, follow: true },
};

// The <html lang> attribute is set per page by the Html component in components/Shell.tsx,
// because English lives at / and Polish at /pl/ and both share this root layout.
export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" suppressHydrationWarning>
      <body>{children}</body>
    </html>
  );
}
