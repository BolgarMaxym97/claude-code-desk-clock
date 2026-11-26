#!/bin/bash
# Smoke test of the clock HTTP API. Usage: api-smoke.sh <host> <token>
set -u
H="${1:?host}"; T="${2:?token}"
pass=0; fail=0
check() { if [[ "$2" == "$3" ]]; then echo "OK   $1"; pass=$((pass+1)); else echo "FAIL $1 (got '$2', want '$3')"; fail=$((fail+1)); fi; }
post() { curl -s -m 3 -o /dev/null -w '%{http_code}' -H "Content-Type: application/json" ${2:+-H "X-Clock-Token: $2"} -d "$3" "http://$H$1"; }
field() { curl -s -m 3 "http://$H/status" | python3 -c "import sys,json;print(json.load(sys.stdin)['$1'])"; }

check "401 without token" "$(post /api/event "" '{"session":"smoke","event":"SessionStart"}')" 401
check "400 on bad json" "$(post /api/event "$T" '{bad')" 400
check "200 valid event" "$(post /api/event "$T" '{"session":"smoke","event":"SessionStart","project":"smoke"}')" 200
for i in $(seq 1 50); do post /api/event "$T" '{"session":"smoke","event":"PreToolUse","tool":"Bash","target":"make"}' >/dev/null; done
check "50 events -> 1 session" "$(field sessions)" 1
check "screen Working" "$(field screen)" Working
check "200 limits" "$(post /api/limits "$T" '{"h5":{"pct":250,"resets_at":0},"d7":{"pct":-3,"resets_at":0}}')" 200
check "h5 clamped to 100" "$(field h5)" 100
check "d7 clamped to 0" "$(field d7)" 0
post /api/event "$T" '{"session":"smoke","event":"SessionEnd"}' >/dev/null
check "SessionEnd removes" "$(field sessions)" 0
echo "passed $pass, failed $fail"; [[ $fail == 0 ]]
