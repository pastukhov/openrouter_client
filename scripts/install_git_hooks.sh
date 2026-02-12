#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
hooks_dir="$repo_root/.git/hooks"
hook_path="$hooks_dir/commit-msg"
validator="$repo_root/scripts/validate_conventional_commit.sh"

if [[ ! -x "$validator" ]]; then
  chmod +x "$validator"
fi

mkdir -p "$hooks_dir"
cat >"$hook_path" <<EOF
#!/usr/bin/env bash
set -euo pipefail
"$validator" "\$1"
EOF
chmod +x "$hook_path"

echo "Installed commit-msg hook at $hook_path"
