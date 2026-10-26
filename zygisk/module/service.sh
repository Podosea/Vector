# Extract the directory path and change directory 
MODDIR="${0%/*}"
cd "$MODDIR" || exit 1

# Start the daemon in a private mount namespace, so the tmpfs it mounts over
# /data/resource-cache cannot propagate back into the system.
#
# `unshare --propagation slave` is a util-linux spelling. toybox 0.8.13 — what Android 15
# and older ship — has no such option and dies with "Unknown option 'propagation'", so the
# daemon is never started at all: the module installs, Zygisk loads it, and then nothing
# listens on the socket. Measured on 2410DPN6CC (toybox 0.8.13-android): the modern
# spelling returns 1, plain `-m` plus a recursive-private remount returns 0 and the tmpfs
# stays inside the namespace (checked from the parent `/proc/self/mountinfo`).
if unshare --propagation slave -m true 2>/dev/null; then
    unshare --propagation slave -m "$MODDIR/daemon" --system-server-max-retry=3 "$@" &
else
    # toybox mount has no propagation flags either; busybox does, and every root solution
    # this module supports ships one. Both are attempted because this stays best-effort: a
    # namespace that leaks one tmpfs is ugly, a daemon that never starts is fatal.
    unshare -m sh -c '
        mount --make-rprivate / 2>/dev/null || busybox mount --make-rprivate / 2>/dev/null
        exec "$0" "$@"
    ' "$MODDIR/daemon" --system-server-max-retry=3 "$@" &
fi
