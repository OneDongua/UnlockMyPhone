#!/system/bin/sh

MODDIR="${0%/*}"
DAEMON=/system/bin/unlockd
LOGFILE=/data/adb/unlockd.log

# Start each boot with a fresh log.
: > "$LOGFILE"

# service.sh runs during late_start. Wait until Android has completed booting
# so that the network stack and input services are available.
until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

if [ ! -f "$DAEMON" ]; then
    echo "unlockd: executable not found: $DAEMON" >> "$LOGFILE"
    exit 1
fi

# Do not start a second copy when the service script is triggered manually.
if pidof unlockd >/dev/null 2>&1; then
    exit 0
fi

umask 077
echo "unlockd: starting $(date)" >> "$LOGFILE"
nohup "$DAEMON" < /dev/null >> "$LOGFILE" 2>&1 &
