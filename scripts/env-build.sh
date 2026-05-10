# Source before building (copy only the first line into your terminal):
#   source scripts/env-build.sh
#
# Sets DEVKITPRO / DEVKITARM, PATH for firmtool + makerom in ~/bin, etc.

export PATH="${HOME}/bin:${HOME}/Library/Python/3.9/bin:${PATH}"

# Required by $(DEVKITPRO)/devkitARM/3ds_rules (sysmodules, arm11, ...).
if [ -d /opt/devkitpro ]; then
	export DEVKITPRO="/opt/devkitpro"
	export DEVKITARM="${DEVKITARM:-/opt/devkitpro/devkitARM}"
	export PATH="${DEVKITARM}/bin:${DEVKITPRO}/tools/bin:${DEVKITPRO}/portlibs/3ds/bin:${PATH}"
fi

# Official devkitPro profile snippet (if the installer created it).
if [ -f /etc/profile.d/devkit-env.sh ]; then
	# shellcheck source=/dev/null
	. /etc/profile.d/devkit-env.sh
fi
