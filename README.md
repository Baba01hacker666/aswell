<p align="center">
  <h1 align="center">⚡ Aswell</h1>
  <p align="center">
    <strong>A modern, high-performance POSIX shell with declarative HTML/CSS styling & 24-bit TrueColor.</strong><br/>
    Built from scratch in C++20 — Zero Electron, Zero Node.js, Zero Web Bloat.
  </p>
</p>

<p align="center">
  <a href="https://github.com/Baba01hacker666/aswell/actions"><img src="https://github.com/Baba01hacker666/aswell/actions/workflows/ci.yml/badge.svg" alt="CI / CD" /></a>
  <a href="docs/POSIX_COMPLIANCE.md"><img src="https://img.shields.io/badge/POSIX-Compatible-blue.svg?style=flat-square" alt="POSIX Compatible" /></a>
  <a href="#"><img src="https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg?style=flat-square" alt="C++20" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-purple.svg?style=flat-square" alt="License" /></a>
  <a href="#"><img src="https://img.shields.io/badge/TrueColor-24--bit-orange.svg?style=flat-square" alt="24-bit TrueColor" /></a>
  <a href="#"><img src="https://img.shields.io/badge/Platform-Linux%20%7C%20Termux%20(Android)-lightgrey.svg?style=flat-square" alt="Platform" /></a>
</p>

---

## 🚀 Quick Install

Install the prebuilt binary directly (no compilation required):

```bash
curl -fsSL https://raw.githubusercontent.com/Baba01hacker666/aswell/master/install.sh | bash
```

> **📱 On Android (Termux):**
> ```bash
> pkg update && pkg install curl tar -y
> curl -fsSL https://raw.githubusercontent.com/Baba01hacker666/aswell/master/install.sh | bash
> ```

The installer automatically detects your architecture (`x86_64` or `aarch64`), downloads the precompiled binary from GitHub Releases, installs it to `/usr/local/bin` (or `$PREFIX/bin` in Termux), and registers it in `/etc/shells`.

---

## ⚡ Quick Start

```bash
# Launch Aswell
aswell

# Open the interactive configuration menu
aswell config

# Pick a theme (modern, cyberpunk, nord, dracula, minimal, powerline)
aswell theme list
aswell theme set cyberpunk

# Use rich TrueColor and gradients in your scripts
color gradient cyan magenta "Hello, World!"
color rainbow "Vibrant rainbow text"
```

---

## ✨ Why Aswell?

- 🐚 **Rock-Solid POSIX Execution**: Complete IEEE Std 1003.1-2017 standard execution. Runs everyday shell scripts, automation pipelines, loops, functions, redirections, brace expansion, and job control.
- 🎨 **HTML & CSS Terminal Styling**: Style your prompt with semantic HTML tags (`<user>`, `<directory>`, `<git>`, `<status>`) and standard CSS (`color`, `border`, `padding`, `animation`).
- 🌈 **24-bit TrueColor Everywhere**: Builtin `color` command, hex colors (`#ff79c6`), gradients, rainbows, and automatic color flags for everyday CLI tools.
- ⌨️ **Aswell Line Editor (ALE)**: Real-time syntax highlighting, history autosuggestions, fuzzy tab completion, reverse search (`Ctrl+R`), and both Emacs & Vi modes.
- ⚡ **Pure Native Speed**: Zero web engine bloat. Renders directly to ANSI escape codes in microseconds.

---

## 🎨 HTML & CSS Prompt Example

Your prompt is defined in `~/.config/aswell/prompt.html` with styles in `theme.css`:

```html
<prompt>
  <user />
  <directory />
  <git />
  <status />
</prompt>
```

```css
user      { color: #ff79c6; font-weight: bold; }
directory { color: #8be9fd; padding: 0 1ch; }
git       { color: #50fa7b; }
status    { color: #ff5555; }
```

---

## 🛠️ Building from Source (Optional)

If you want to build manually from source:

```bash
# Clone and build
git clone https://github.com/Baba01hacker666/aswell.git
cd aswell
make -j$(nproc)

# Install system-wide
sudo make install
```

Run tests:
```bash
make test
```

---

## 📖 Documentation

- [System Architecture](docs/ARCHITECTURE.md)
- [Configuration & Settings](docs/CONFIGURATION.md)
- [CSS & HTML Reference](docs/CSS_REFERENCE.md)
- [POSIX Conformance Matrix](docs/POSIX_COMPLIANCE.md)
- [Plugin System](docs/PLUGINS.md)

---

## 📜 License

MIT License. See [LICENSE](LICENSE) for details.
