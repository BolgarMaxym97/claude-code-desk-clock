#!/bin/sh
# Build clock firmware in Docker; prints RAM/Flash usage and result.
cd "$(dirname "$0")/.." && docker run --rm -v "$PWD":/w -v pio-cache:/root/.platformio -w /w python:3.12-slim \
  sh -c "pip install -q platformio 2>/dev/null && pio run -e clock 2>&1" | grep -E 'RAM:|Flash:|error|SUCCESS|FAILED'
