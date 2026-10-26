#!/usr/bin/env bash
# One-time repository protection for plain-browser/plain-browser. Needs a token with repository
# "Administration: write" (rulesets) and account "SSH signing keys: write"; the fine-grained token used by the
# assistant cannot do this, so run it yourself:
#   gh auth refresh -h github.com -s admin:ssh_signing_key   # classic token; or create a fine-grained one
#   bash scripts/github-protect-main.sh
# Idempotent: skips a ruleset or key that already exists.
set -euo pipefail
REPO="${REPO:-plain-browser/plain-browser}"
KEY="${SIGNING_KEY:-$HOME/.ssh/id_ed25519.pub}"

# 1. Ruleset on the default branch: no deletion, no force-push, linear history, signed commits.
#    No required pull-request review: a single maintainer could not merge anything. Add
#    {"type":"pull_request","parameters":{"required_approving_review_count":1,...}} once there is a second reviewer.
if gh api "repos/$REPO/rulesets" --jq '.[].name' | grep -qx protect-main; then
  echo "ruleset protect-main already exists"
else
  gh api -X POST "repos/$REPO/rulesets" --input - <<'JSON'
{
  "name": "protect-main",
  "target": "branch",
  "enforcement": "active",
  "conditions": { "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] } },
  "rules": [
    { "type": "deletion" },
    { "type": "non_fast_forward" },
    { "type": "required_signatures" },
    { "type": "required_linear_history" }
  ]
}
JSON
  echo "ruleset protect-main created"
fi

# 2. Register the SSH public key as a *signing* key (separate from authentication keys), so GitHub shows
#    commits signed with it as Verified. git in this checkout is already configured to sign with it
#    (gpg.format=ssh, commit.gpgsign=true; see git config --local --list).
if gh ssh-key list --json title 2>/dev/null | grep -q 'plain-browser commit signing'; then
  echo "signing key already registered"
else
  gh ssh-key add "$KEY" --type signing --title "plain-browser commit signing"
  echo "signing key registered"
fi

# 3. Settings that have no API for personal repos: enable in Settings > Code security:
#    private vulnerability reporting, Dependabot alerts, secret scanning + push protection.
echo "Now enable in https://github.com/$REPO/settings/security_analysis: private vulnerability reporting, Dependabot alerts, secret scanning, push protection."
