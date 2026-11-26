#!/bin/sh
# Claude Clock - installer for macOS and Linux.
#   curl -fsSL https://raw.githubusercontent.com/BolgarMaxym97/claude-code-desk-clock/main/install/install.sh | sh
# Downloads the claude-clock program for this computer from the latest GitHub release, checks its sha256,
# puts it in ~/.local/bin and runs `claude-clock setup` (find the clock, pair, register Claude Code hooks).
set -eu
REPO="BolgarMaxym97/claude-code-desk-clock"
DIR="${CLAUDE_CLOCK_BIN_DIR:-$HOME/.local/bin}"

case "$(uname -s)" in
  Darwin) os=darwin ;;
  Linux) os=linux ;;
  *) echo "Unsupported system: $(uname -s). On Windows use install.ps1." >&2; exit 1 ;;
esac
case "$(uname -m)" in
  x86_64|amd64) arch=amd64 ;;
  arm64|aarch64) arch=arm64 ;;
  *) echo "Unsupported CPU: $(uname -m)" >&2; exit 1 ;;
esac

name="claude-clock-${os}-${arch}"
base="https://github.com/${REPO}/releases/latest/download"
tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT

echo "Downloading ${name}..."
curl -fsSL "${base}/${name}" -o "${tmp}/${name}"
curl -fsSL "${base}/checksums.txt" -o "${tmp}/checksums.txt"
want="$(grep " ${name}\$" "${tmp}/checksums.txt" | cut -d' ' -f1)"
if command -v sha256sum >/dev/null 2>&1; then have="$(sha256sum "${tmp}/${name}" | cut -d' ' -f1)"
else have="$(shasum -a 256 "${tmp}/${name}" | cut -d' ' -f1)"; fi
[ -n "${want}" ] && [ "${want}" = "${have}" ] || { echo "Checksum mismatch, not installing." >&2; exit 1; }

mkdir -p "${DIR}"
install -m 755 "${tmp}/${name}" "${DIR}/claude-clock"
echo "Installed ${DIR}/claude-clock"
case ":$PATH:" in *":${DIR}:"*) ;; *) echo "Tip: add ${DIR} to your PATH to type 'claude-clock' anywhere." ;; esac

# Piped into sh, stdin is the script: questions must come from the terminal.
if (: </dev/tty) 2>/dev/null; then "${DIR}/claude-clock" setup "$@" </dev/tty
else echo "Now run: ${DIR}/claude-clock setup"; fi
