#!/bin/sh
# Copyright 2026 Xeno Tech
# SPDX-License-Identifier: Apache-2.0
# Development-only Linux PC session supervisor. Not a console bootloader.
set -u
umask 077
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd) || exit 1
ASTRA_BIN=${ASTRA_BIN:-"$ROOT/build/astra"}
ASTRA_SHELL_BIN=${ASTRA_SHELL_BIN:-"$ROOT/shell/Astra.Shell/bin/Release/net8.0/Astra.Shell"}
if [ ! -x "$ASTRA_BIN" ]; then
  echo "Astra session: missing executable Astra Core: $ASTRA_BIN" >&2
  exit 2
fi
if [ ! -x "$ASTRA_SHELL_BIN" ]; then
  echo "Astra session: missing executable Shell: $ASTRA_SHELL_BIN" >&2
  echo "Build with: dotnet publish shell/Astra.Shell -c Release" >&2
  exit 2
fi
export ASTRA_BIN
# Avoid indefinite restart loops; a failed shell must return control to the session owner.
tries=0
while [ "$tries" -lt 3 ]; do
  tries=$((tries + 1))
  echo "Astra session: starting Shell ($tries/3)" >&2
  "$ASTRA_SHELL_BIN"
  status=$?
  if [ "$status" -eq 0 ]; then exit 0; fi
  echo "Astra session: Shell exited with $status" >&2
done
echo "Astra session: entering recovery; start a regular Linux desktop or terminal." >&2
exit 1
