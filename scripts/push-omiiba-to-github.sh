#!/bin/sh
# Push this fork to your own GitHub repo (one-time setup + push).
# Prerequisites:
#   1. brew install gh   (already done if you have /opt/homebrew/bin/gh)
#   2. gh auth login     (once — follow the browser/device flow)
#   3. Create nothing on GitHub first, OR use --existing if the repo already exists.
#
# Usage:
#   ./scripts/push-omiiba-to-github.sh              # repo name Omiiba3DS under your account
#   ./scripts/push-omiiba-to-github.sh OtherName   # repo name OtherName under your account
#   EXISTING=1 ./scripts/push-omiiba-to-github.sh   # empty repo already on GitHub; only remote + push

set -eu
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

REPO_NAME="${1:-Omiiba3DS}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v gh >/dev/null 2>&1; then
	echo "Install GitHub CLI:  brew install gh" >&2
	exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
	echo "Not logged in. Run this first in your terminal:" >&2
	echo "  gh auth login" >&2
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
		echo "Renaming origin -> upstream (Luma3DS reference)"
		git remote rename origin upstream
		;;
	esac
fi

if git remote get-url origin >/dev/null 2>&1; then
	OURL="$(git remote get-url origin)"
	case "$OURL" in
	*"${FULL}"*) ;;
	*)
		echo "Remote 'origin' already points to: $OURL" >&2
		echo "Remove or rename it, then re-run. Expected: https://github.com/${FULL}.git" >&2
		exit 1
		;;
	esac
else
	if [ "${EXISTING:-0}" = 1 ]; then
		echo "Adding origin -> https://github.com/${FULL}.git"
		git remote add origin "https://github.com/${FULL}.git"
	else
		echo "Creating https://github.com/${FULL} (empty) and wiring origin..."
		gh repo create "$REPO_NAME" --public --description "Educational fork of Luma3DS (Omiiba3DS)"
		git remote add origin "https://github.com/${FULL}.git"
	fi
fi

# Ensure tag exists (matches arm9/Makefile default v1.3.0)
if git rev-parse v1.3.0 >/dev/null 2>&1; then
	:
else
	git tag -a v1.3.0 -m "Omiiba3DS v1.3.0" || true
fi

echo "Pushing master + tags to origin (${FULL})..."
git push -u origin master
git push origin --tags

echo "Done: https://github.com/${FULL}"
