#!/usr/bin/env bash
set -euo pipefail

if ! command -v gh >/dev/null 2>&1; then
  echo "ERROR: GitHub CLI (gh) is required." >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "ERROR: jq is required." >&2
  exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
  echo "ERROR: gh is not authenticated. Run: gh auth login" >&2
  exit 1
fi

remote_url="$(git remote get-url origin)"
if [[ "$remote_url" =~ github.com[:/]([^/]+)/([^/.]+)(\.git)?$ ]]; then
  owner="${BASH_REMATCH[1]}"
  repo="${BASH_REMATCH[2]}"
else
  echo "ERROR: origin does not point to a GitHub repository: $remote_url" >&2
  exit 1
fi

default_branch="$(gh api "repos/$owner/$repo" --jq '.default_branch')"

payload_file="$(mktemp)"
trap 'rm -f "$payload_file"' EXIT

jq -n \
  --arg branch "$default_branch" \
  '{
    required_status_checks: {
      strict: true,
      contexts: []
    },
    enforce_admins: true,
    required_pull_request_reviews: null,
    restrictions: null,
    required_linear_history: true,
    allow_force_pushes: false,
    allow_deletions: false,
    block_creations: false,
    required_conversation_resolution: true,
    lock_branch: false,
    allow_fork_syncing: true
  }' >"$payload_file"

gh api \
  --method PUT \
  -H "Accept: application/vnd.github+json" \
  "repos/$owner/$repo/branches/$default_branch/protection" \
  --input "$payload_file" >/dev/null

# Set required status checks explicitly. This endpoint is authoritative for contexts.
checks_payload_file="$(mktemp)"
trap 'rm -f "$payload_file" "$checks_payload_file"' EXIT
cat >"$checks_payload_file" <<'EOF'
["python-schema-tests","esphome-config-smoke","idf-unit-build"]
EOF

gh api \
  --method PUT \
  -H "Accept: application/vnd.github+json" \
  "repos/$owner/$repo/branches/$default_branch/protection/required_status_checks/contexts" \
  --input "$checks_payload_file" >/dev/null

echo "Branch protection updated for $owner/$repo:$default_branch"
