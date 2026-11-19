#!/bin/sh
# fix-airmon-ng.sh — repair airmon-ng's infinite-recursion crash on non-TTY stdin.
#
# Symptom: under NetHunter, `airmon-ng` (and `wifite`, which calls it) loops forever
#   printing "Invalid input. Yes, or no? [y/n]" and eventually crashes with
#   "Maximum function recursion depth (1000) reached" — so wifite reports
#   "airmon-ng did not find any wireless interfaces".
#
# Cause: airmon-ng's yesorno() does `read -r input`; when stdin is NOT a TTY (how
#   wifite / any script runs it) read hits EOF, $input is empty, it falls through to
#   the `*)` case and recurses into yesorno() forever. The prompt is triggered by
#   handleLostPhys(), because the internal wlan0/phy0 has no `device/net` visible from
#   inside the chroot ("Found phy0 with no interfaces assigned, ...").
#
# Fix: make yesorno() answer "no" on EOF instead of recursing —
#   `read -r input`  ->  `read -r input || return 1`
# After this, airmon-ng auto-skips phy0 ("PHY phy0 will remain lost.") and correctly
# lists + starts monitor on wlan1 (RTL8187) and wlan2 (RTL8822BU). This touches only
# the y/n prompt handling; every other airmon-ng behaviour is unchanged.
#
# Usage (run as root):
#   # from Android (patches the chroot's copy — default path shown):
#   sh fix-airmon-ng.sh [/data/local/nhsystem/kali-arm64]
#   # or from inside the Kali chroot:
#   sh fix-airmon-ng.sh /
#
set -e
CH="${1:-/data/local/nhsystem/kali-arm64}"
AM="$CH/usr/sbin/airmon-ng"
[ -f "$AM" ] || AM="$CH/usr/bin/airmon-ng"
if [ ! -f "$AM" ]; then
  echo "airmon-ng not found under $CH (looked in usr/sbin and usr/bin)"
  echo "pass the chroot root as arg 1, e.g.: sh $0 /data/local/nhsystem/kali-arm64"
  exit 1
fi

if grep -q 'read -r input || return 1' "$AM"; then
  echo "already patched: $AM"
  exit 0
fi

# one-time backup
[ -f "$AM.orig" ] || cp "$AM" "$AM.orig"

# busybox sed is present in NetHunter; use plain sed if that is what's on PATH
if command -v sed >/dev/null 2>&1; then SED="sed"; else SED="busybox sed"; fi
$SED -i 's/^\([[:space:]]*\)read -r input$/\1read -r input || return 1/' "$AM"

if grep -q 'read -r input || return 1' "$AM"; then
  echo "patched OK: $AM"
  echo "backup kept at: $AM.orig"
  echo "now: airmon-ng / airmon-ng start wlanN / wifite enumerate both cards"
else
  echo "patch FAILED — restoring backup"
  cp "$AM.orig" "$AM"
  exit 1
fi
