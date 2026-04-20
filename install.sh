#!/usr/bin/env sh
# Install openmv_mcp_server on Linux from GitHub Releases.
# macOS: use `brew install SingTown/openmv/openmv-mcp` instead.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/SingTown/openmv-mcp/main/install.sh | sh
#   curl -fsSL https://raw.githubusercontent.com/SingTown/openmv-mcp/main/install.sh | sh -s -- --version 2.3.1
#   curl -fsSL https://raw.githubusercontent.com/SingTown/openmv-mcp/main/install.sh | sh -s -- --dir ~/.local/bin

set -eu

REPO="SingTown/openmv-mcp"
BIN_NAME="openmv_mcp_server"
VERSION=""
INSTALL_DIR=""

while [ $# -gt 0 ]; do
    case "$1" in
        --version) VERSION="${2#v}"; shift 2 ;;
        --dir)     INSTALL_DIR="$2";  shift 2 ;;
        -h|--help)
            sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
done

err() { echo "error: $*" >&2; exit 1; }

[ "$(uname -s)" = "Linux" ] || err "this script is for Linux. On macOS: brew install SingTown/openmv/openmv-mcp"

case "$(uname -m)" in
    x86_64|amd64) arch_tag=x86_64 ;;
    arm64|aarch64) arch_tag=arm64 ;;
    *) err "unsupported arch: $(uname -m)" ;;
esac

if [ -z "$VERSION" ]; then
    api="https://api.github.com/repos/$REPO/releases/latest"
    tag=$(curl -fsSL "$api" | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n1)
    [ -n "$tag" ] || err "failed to resolve latest release from $api"
    VERSION="${tag#v}"
fi

asset="${BIN_NAME}-${VERSION}-linux-${arch_tag}"
url="https://github.com/$REPO/releases/download/v${VERSION}/${asset}"

if [ -z "$INSTALL_DIR" ]; then
    if [ -w /usr/local/bin ] 2>/dev/null; then
        INSTALL_DIR=/usr/local/bin
        SUDO=""
    elif command -v sudo >/dev/null 2>&1 && [ -d /usr/local/bin ]; then
        INSTALL_DIR=/usr/local/bin
        SUDO="sudo"
    else
        INSTALL_DIR="$HOME/.local/bin"
        SUDO=""
        mkdir -p "$INSTALL_DIR"
    fi
else
    SUDO=""
    mkdir -p "$INSTALL_DIR"
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "Downloading $asset ..."
curl -fL --progress-bar -o "$tmp/$BIN_NAME" "$url" \
    || err "download failed: $url"

chmod +x "$tmp/$BIN_NAME"
$SUDO mv "$tmp/$BIN_NAME" "$INSTALL_DIR/$BIN_NAME"

echo
echo "Installed: $INSTALL_DIR/$BIN_NAME"
case ":$PATH:" in
    *:"$INSTALL_DIR":*) "$INSTALL_DIR/$BIN_NAME" --version ;;
    *) echo "Note: $INSTALL_DIR is not in PATH. Add it, or run:"
       echo "  $INSTALL_DIR/$BIN_NAME --version" ;;
esac
