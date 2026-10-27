#!/usr/bin/env bash
# Starts a private X server (Xvfb :97) with a minimal window manager, so tests don't open windows
# on the user's display. Idempotent. Use with DISPLAY=:97.
if ! pgrep -x Xvfb >/dev/null; then
  setsid Xvfb :97 -screen 0 1920x1080x24 -nolisten tcp -nolisten local -ac > /tmp/xvfb97.log 2>&1 < /dev/null &
  sleep 2
fi
if ! pgrep -x twm >/dev/null; then
  DISPLAY=:97 setsid /nix/store/l2ww36smkqlsf11biyrwvqsr832k1j8w-tab-window-manager-1.0.13.1/bin/twm -f /home/ryan/src/angle/out/twmrc > /tmp/twm97.log 2>&1 < /dev/null &
  sleep 1
fi
