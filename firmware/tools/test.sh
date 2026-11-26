#!/bin/sh
# Run model unit tests in Docker (host has no gcc toolchain for PlatformIO native).
cd "$(dirname "$0")/.." && docker run --rm -v "$PWD":/w -v pio-cache:/root/.platformio -w /w python:3.12 \
  sh -c "pip install -q platformio 2>/dev/null && pio test -e native 2>&1" | grep -E '\[(PASSED|FAILED)\]|:FAIL|error:|test cases' 
