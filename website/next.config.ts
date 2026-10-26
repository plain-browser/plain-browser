import type { NextConfig } from "next";

// Static export: the site is a set of files on Google Cloud Storage behind Cloudflare.
// trailingSlash makes every route a directory with index.html, which GCS serves as the
// bucket's MainPageSuffix, so /download/ works without any server logic.
const nextConfig: NextConfig = {
  output: "export",
  trailingSlash: true,
  images: { unoptimized: true },
  reactStrictMode: true,
};

export default nextConfig;
