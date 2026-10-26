#!/usr/bin/env bash
# Build the site and sync it to the Google Cloud Storage bucket that Cloudflare fronts.
# Usage: BUCKET=plain-browser.app npm run deploy
set -euo pipefail
cd "$(dirname "$0")/.."
BUCKET="${BUCKET:-plain-browser.app}"

npm run build

# Hashed assets under _next/static never change: cache for a year.
# Everything else (HTML, sitemap, icons) is revalidated by Cloudflare per the rule in README.md.
gcloud storage rsync out "gs://$BUCKET" --recursive --delete-unmatched-destination-objects \
  --exclude='^_next/static/.*'
gcloud storage rsync out/_next/static "gs://$BUCKET/_next/static" --recursive \
  --delete-unmatched-destination-objects \
  --cache-control='public, max-age=31536000, immutable'
gcloud storage objects update "gs://$BUCKET/**.html" --cache-control='public, max-age=300' --quiet
echo "deployed to gs://$BUCKET"
