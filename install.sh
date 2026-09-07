#!/usr/bin/env bash
# ==============================================================================
#  Aswell Shell — Fast Prebuilt Binary Installer
#
#  Usage:
#    curl -fsSL https://raw.githubusercontent.com/Baba01hacker666/aswell/master/install.sh | bash
#
#  Environment Overrides:
#    VERSION=v1.0.1           Specific version tag (default: latest release)
#    PREFIX=/custom/path      Installation directory (default: /usr/local or ~/.local)
# ==============================================================================

set -e

# Detect terminal colors
if [ -t 1 ]; then
    BOLD="\033[1m"
    RESET="\033[0m"
    GREEN="\033[38;2;80;250;123m"
    CYAN="\033[38;2;0;240;255m"
    YELLOW="\033[38;2;255;184;108m"
    RED="\033[38;2;255;85;85m"
    PURPLE="\033[38;2;189;147;249m"
    DIM="\033[2m"
else
    BOLD="" RESET="" GREEN="" CYAN="" YELLOW="" RED="" PURPLE="" DIM=""
fi

log_info()    { printf "${CYAN}==>${RESET} ${BOLD}%s${RESET}\n" "$1"; }
log_step()    { printf "  ${GREEN}✓${RESET} %s\n" "$1"; }
log_warn()    { printf "  ${YELLOW}⚠ %s${RESET}\n" "$1"; }
log_error()   { printf "${RED}✖ Error:${RESET} %s\n" "$1" >&2; }
log_header()  {
    printf "\n"
    printf "${CYAN}╔══════════════════════════════════════════════════════════════════════╗${RESET}\n"
    printf "${CYAN}║${RESET}  ${BOLD}${PURPLE}⚡ ASWELL SHELL INSTALLER${RESET}                                           ${CYAN}║${RESET}\n"
    printf "${CYAN}║${RESET}  ${DIM}Prebuilt High-Performance POSIX Shell with TrueColor Engine${RESET}         ${CYAN}║${RESET}\n"
    printf "${CYAN}╚══════════════════════════════════════════════════════════════════════╝${RESET}\n"
    printf "\n"
}

log_header

# 1. Platform validation (Linux & Termux)
OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
if [ "$OS" != "linux" ]; then
    log_error "Aswell prebuilt binaries currently support Linux (x86_64 and aarch64) and Android/Termux."
    exit 1
fi

# Detect Termux environment
IS_TERMUX=0
if [ -n "$TERMUX_VERSION" ] || [ -d "/data/data/com.termux" ] || [ "${PREFIX:-}" = "/data/data/com.termux/files/usr" ]; then
    IS_TERMUX=1
    log_step "Detected Termux environment"
fi

# 2. Architecture detection (x86_64 or aarch64)
ARCH_RAW="$(uname -m)"
case "$ARCH_RAW" in
    x86_64|amd64)
        ARCH="x86_64"
        ;;
    aarch64|arm64|armv8*|armv9*)
        ARCH="aarch64"
        ;;
    *)
        log_error "Unsupported architecture: $ARCH_RAW. Supported architectures: x86_64, aarch64."
        exit 1
        ;;
esac
log_step "Detected architecture: ${ARCH}"

# 3. Determine download URL
GITHUB_REPO="Baba01hacker666/aswell"
if [ "$IS_TERMUX" -eq 1 ]; then
    ARCHIVE_NAME="aswell-termux-${ARCH}.tar.gz"
else
    ARCHIVE_NAME="aswell-linux-${ARCH}.tar.gz"
fi

if [ -n "$VERSION" ]; then
    DOWNLOAD_URL="https://github.com/${GITHUB_REPO}/releases/download/${VERSION}/${ARCHIVE_NAME}"
    log_info "Target release: ${VERSION}"
else
    DOWNLOAD_URL="https://github.com/${GITHUB_REPO}/releases/latest/download/${ARCHIVE_NAME}"
    log_info "Target release: latest"
fi

# 4. Download prebuilt package via curl (with wget fallback)
TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'aswell-install')
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

do_download() {
    local url="$1"
    local dest="$2"
    if command -v curl >/dev/null 2>&1; then
        curl -fSL --progress-bar "$url" -o "$dest"
    elif command -v wget >/dev/null 2>&1; then
        wget -q --show-progress "$url" -O "$dest"
    else
        log_error "Neither curl nor wget found. Please install curl or wget."
        exit 1
    fi
}

log_info "Downloading ${ARCHIVE_NAME}..."
if ! do_download "$DOWNLOAD_URL" "$TMP_DIR/$ARCHIVE_NAME"; then
    if [ "$IS_TERMUX" -eq 1 ]; then
        FALLBACK_NAME="aswell-linux-${ARCH}.tar.gz"
        FALLBACK_URL="${DOWNLOAD_URL%/*}/${FALLBACK_NAME}"
        log_warn "Termux-specific package not found, trying ${FALLBACK_NAME}..."
        if ! do_download "$FALLBACK_URL" "$TMP_DIR/$ARCHIVE_NAME"; then
            log_error "Failed to download prebuilt binary from: $DOWNLOAD_URL"
            printf "\nCheck the GitHub Releases page: https://github.com/${GITHUB_REPO}/releases\n"
            exit 1
        fi
    else
        log_error "Failed to download prebuilt binary from: $DOWNLOAD_URL"
        printf "\nCheck the GitHub Releases page: https://github.com/${GITHUB_REPO}/releases\n"
        exit 1
    fi
fi
log_step "Downloaded prebuilt release package"

# 5. Extract prebuilt binaries
log_info "Extracting package..."
tar -xzf "$TMP_DIR/$ARCHIVE_NAME" -C "$TMP_DIR"
if [ ! -f "$TMP_DIR/aswell" ]; then
    log_error "Archive did not contain the 'aswell' binary."
    exit 1
fi
chmod +x "$TMP_DIR/aswell"
if [ -f "$TMP_DIR/aswell-demo" ]; then
    chmod +x "$TMP_DIR/aswell-demo"
fi
log_step "Extracted binaries successfully"

# 6. Determine installation directory
if [ "$IS_TERMUX" -eq 1 ]; then
    INSTALL_PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
    SUDO=""
elif [ -n "$PREFIX" ]; then
    INSTALL_PREFIX="$PREFIX"
    SUDO=""
elif [ "$(id -u)" -eq 0 ]; then
    INSTALL_PREFIX="/usr/local"
    SUDO=""
elif [ -w "/usr/local/bin" ]; then
    INSTALL_PREFIX="/usr/local"
    SUDO=""
elif [ "$NONINTERACTIVE" != "1" ] && command -v sudo >/dev/null 2>&1 && [ -t 0 ]; then
    INSTALL_PREFIX="/usr/local"
    SUDO="sudo"
else
    INSTALL_PREFIX="$HOME/.local"
    SUDO=""
fi

log_info "Installing to ${INSTALL_PREFIX}/bin..."
$SUDO mkdir -p "${INSTALL_PREFIX}/bin"
if command -v install >/dev/null 2>&1; then
    $SUDO install -m 755 "$TMP_DIR/aswell" "${INSTALL_PREFIX}/bin/aswell"
    if [ -f "$TMP_DIR/aswell-demo" ]; then
        $SUDO install -m 755 "$TMP_DIR/aswell-demo" "${INSTALL_PREFIX}/bin/aswell-demo"
    fi
else
    $SUDO cp -f "$TMP_DIR/aswell" "${INSTALL_PREFIX}/bin/aswell"
    $SUDO chmod 755 "${INSTALL_PREFIX}/bin/aswell"
    if [ -f "$TMP_DIR/aswell-demo" ]; then
        $SUDO cp -f "$TMP_DIR/aswell-demo" "${INSTALL_PREFIX}/bin/aswell-demo"
        $SUDO chmod 755 "${INSTALL_PREFIX}/bin/aswell-demo"
    fi
fi
log_step "Installed ${INSTALL_PREFIX}/bin/aswell"

# Also symlink to ~/.local/bin if directory exists
if [ -d "$HOME/.local/bin" ] && [ "${INSTALL_PREFIX}/bin" != "$HOME/.local/bin" ]; then
    ln -sf "${INSTALL_PREFIX}/bin/aswell" "$HOME/.local/bin/aswell" 2>/dev/null || true
fi

# 7. Initialize user configuration & themes
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/aswell"
mkdir -p "$CONFIG_DIR/commands" "$CONFIG_DIR/themes" "$CONFIG_DIR/plugins"

if [ -d "$TMP_DIR/themes" ]; then
    for theme in "$TMP_DIR"/themes/*.css; do
        if [ -f "$theme" ]; then
            fname=$(basename "$theme")
            if [ ! -f "$CONFIG_DIR/themes/$fname" ]; then
                cp "$theme" "$CONFIG_DIR/themes/"
            fi
        fi
    done
    log_step "Initialized ~/.config/aswell/ (custom commands & themes ready)"
fi

# 8. Check shells registration
ASWELL_BIN="${INSTALL_PREFIX}/bin/aswell"
if [ "$IS_TERMUX" -eq 1 ]; then
    SHELLS_FILE="${INSTALL_PREFIX}/etc/shells"
    mkdir -p "${INSTALL_PREFIX}/etc" 2>/dev/null || true
    if [ ! -f "$SHELLS_FILE" ]; then
        touch "$SHELLS_FILE" 2>/dev/null || true
    fi
else
    SHELLS_FILE="/etc/shells"
fi

if [ -f "$SHELLS_FILE" ] && ! grep -qx "$ASWELL_BIN" "$SHELLS_FILE" 2>/dev/null; then
    if [ "$IS_TERMUX" -eq 1 ] || [ "$(id -u)" -eq 0 ]; then
        echo "$ASWELL_BIN" >> "$SHELLS_FILE" 2>/dev/null && \
            log_step "Added $ASWELL_BIN to $SHELLS_FILE" || true
    elif [ -n "$SUDO" ]; then
        echo "$ASWELL_BIN" | $SUDO tee -a "$SHELLS_FILE" >/dev/null 2>&1 && \
            log_step "Added $ASWELL_BIN to $SHELLS_FILE" || true
    fi
fi

# 9. Verify PATH accessibility
case ":$PATH:" in
    *:"${INSTALL_PREFIX}/bin":*) ;;
    *)
        log_warn "${INSTALL_PREFIX}/bin is not in your current \$PATH!"
        printf "     Add it by running or placing in your shell profile:\n"
        printf "       ${BOLD}export PATH=\"${INSTALL_PREFIX}/bin:\$PATH\"${RESET}\n\n"
        ;;
esac

# 10. Summary
printf "\n${GREEN}${BOLD}⚡ Aswell Shell has been installed successfully!${RESET}\n\n"
printf "Quick Start:\n"
printf "  ${BOLD}%-22s${RESET} Launch interactive Aswell session\n" "${ASWELL_BIN}"
printf "  ${BOLD}%-22s${RESET} Open interactive configuration TUI\n" "aswell config"
printf "  ${BOLD}%-22s${RESET} Explore 24-bit TrueColor palette & styles\n" "aswell color list"
printf "  ${BOLD}%-22s${RESET} View installed prompt CSS themes\n" "aswell theme list"
if [ -f "${INSTALL_PREFIX}/bin/aswell-demo" ]; then
    printf "  ${BOLD}%-22s${RESET} Run dynamic UI & 60fps animation engine showcase\n" "aswell-demo"
fi
printf "\nTo switch your default login shell to Aswell:\n"
printf "  ${BOLD}chsh -s %s${RESET}\n\n" "${ASWELL_BIN}"
