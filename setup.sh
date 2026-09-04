#!/usr/bin/env bash
# Forward to Linux setup script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/setup-linux.sh" "$@"
