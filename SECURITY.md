# Security policy

## Reporting a vulnerability

Please do not open a public issue for a security problem. Use GitHub's private reporting for this
repository: **Security tab, then "Report a vulnerability"**. Include the Plain Browser version, steps to reproduce and
what you expect an attacker to gain. You should get a first answer within a week.

If the bug is in Chromium itself (it reproduces in an unmodified ungoogled-chromium or Chromium of the same version),
report it to the Chromium project; we will pick up their fix through our upstream tracking.

## What we keep up to date

Plain Browser is a thin layer on top of ungoogled-chromium, so most security fixes arrive with a new Chromium release.
Our aim is to ship a new version within a few days of every upstream stable tag (`.github/workflows/upstream-watch.yml`
opens an issue when a newer tag appears). Only the latest release is supported.

## Verifying a release

Releases carry a `SHA256SUMS.txt` and a keyless cosign bundle per file (`.github/workflows/sign-release.yml`):

```
cosign verify-blob --bundle <file>.bundle \
  --certificate-identity-regexp 'https://github.com/carriera/plain-browser/.github/workflows/sign-release.yml@.*' \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com <file>
sha256sum -c SHA256SUMS.txt
```

The signature shows who published the file and that it was not changed afterwards. It does not prove how the binary was
built, and Windows code signing (Authenticode) is a separate, still open item: until it is done, SmartScreen and Smart App
Control will warn about or refuse our installers. No release in this repository is signed yet.
