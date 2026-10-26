import type { MetadataRoute } from "next";
import { site } from "@/lib/site";

export const dynamic = "force-static";

export default function sitemap(): MetadataRoute.Sitemap {
  const pages = ["/", "/download/", "/privacy/", "/security/", "/pl/", "/pl/download/", "/pl/privacy/", "/pl/security/"];
  return pages.map((p) => ({ url: `${site.url}${p}`, lastModified: new Date() }));
}
