#!/bin/sh
# Push Omiiba3DS to your GitHub account.
#
# First run this once in Terminal, alone on its own line, and finish the wizard:
#   /opt/homebrew/bin/gh auth login
#
# Then run this script from the repo root:
#   ./scripts/push-omiiba-to-github.sh
#
# If the empty repo Omiiba3DS already exists on GitHub under your account:
#   EXISTING=1 ./scripts/push-omiiba-to-github.sh
#
# Optional: different repo name as first argument:
#   ./scripts/push-omiiba-to-github.sh MyRepoName

set -eu
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

REPO_NAME="${1:-Omiiba3DS}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v gh >/dev/null 2>&1; then
	echo "Install GitHub CLI: brew install gh" >&2
	exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
	echo "You are not logged in to GitHub in this terminal." >&2
	echo "Run this command alone, complete the prompts, then run this script again:" >&2
	echo "  /opt/homebrew/bin/gh auth login" >&2
	exit 1
fi

OWNER="$(gh api user -q .login)"
FULL="${OWNER}/${REPO_NAME}"

if git remote get-url upstream >/dev/null 2>&1; then
	:
elif git remote get-url origin >/dev/null 2>&1; then
	URL="$(git remote get-url origin)"
	case "$URL" in
	*LumaTeam/Luma3DS*)
		echo "Renaming git remote origin to upstream (keeps Luma3DS as reference)."
		git remote rename origin upstream
		;;
	esac
fi

if git remote get-url origin >/dev/null 2>&1; then
	OURL="$(git remote get-url origin)"
	case "$OURL" in
	*"${FULL}"*)
		echo "Remote origin already points to your repo."
		;;
	*)
		echo "Remote origin points to: $OURL" >&2
		echo "Expected your repo: https://github.com/${FULL}.git" >&2
		echo "Fix with: git remote remove origin   then re-run this script." >&2
		exit 1
		;;
	esac
else
	if [ "${EXISTING:-0}" = 1 ]; then
		echo "Adding origin -> https://github.com/${FULL}.git"
		git remote add origin "https://github.com/${FULL}.git"
	else
		echo "Creating https://github.com/${FULL} and pushing master in one step..."
		gh repo create "$REPO_NAME" --public --source=. --remote=origin \
			--description "Educational fork of Luma3DS" --push
	fi
fi

if ! git rev-parse v1.3.0 >/dev/null 2>&1; then
	git tag -a v1.3.0 -m "Omiiba3DS v1.3.0" || true
fi

echo "Pushing master and tags to origin..."
git push -u origin master
git push origin --tags || true

echo "Done: https://github.com/${FULL}"
