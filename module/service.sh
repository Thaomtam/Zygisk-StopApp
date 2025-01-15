#!/system/bin/sh

MODDIR=${0%/*}

# Thiết lập quyền
chmod 0644 "$MODDIR/blacklist.json"
chmod 0644 "$MODDIR/whitelist.json"
chmod 0644 "$MODDIR/classes.dex"

# Tùy ý
