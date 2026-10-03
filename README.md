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

# Open the interactive configuration editor (live prompt preview, no restarts)
aswell config

# Or script it — every setting is reachable, validated and applied instantly
aswell config list                    # what differs from the defaults
aswell config set show_git false      # change one setting for real, persisted
aswell config toggle vi_mode          # flip a boolean
aswell config help                    # what every setting does

# Preview a theme, switch to it, and keep editing your own
aswell theme list                     # modern, cyberpunk, matrix, nord, minimal, dracula, powerline + yours
aswell theme preview --all
aswell theme set cyberpunk
aswell theme new mytheme --from dracula   # themes/mytheme.css; hot-reloaded as you edit it

# Diagnose a setup (great in CI for dotfiles)
aswell doctor

# Use rich TrueColor and gradients in your scripts
color gradient cyan magenta "Hello, World!"
color rainbow "Vibrant rainbow text"
```

> **Everything is live.** `aswell config set`, `aswell theme set` and file edits to
> `theme.css`, `themes/*.css`, `prompt.html` or `config.txt` apply to your running
> shell before the next prompt is drawn — no restarts, and `aswell reload` forces it.

---

## ✨ Why Aswell?

- 🐚 **Rock-Solid POSIX Execution**: Complete IEEE Std 1003.1-2017 standard execution. Runs everyday shell scripts, automation pipelines, loops, functions, redirections, brace expansion, and job control.
- 🎨 **HTML & CSS Terminal Styling**: Style your prompt with semantic HTML tags (`<user>`, `<directory>`, `<git>`, `<status>`, `<time>`, `<date>`, `<rprompt>`) and standard CSS (`color`, `background`, `border`, `padding`, `animation`).
- 🧩 **One Settings Hub, Zero Guessing**: 22 documented settings behind `aswell config list/get/set/toggle/help`, with aliases, validation, "did you mean" recovery, JSON output for scripting, and an interactive TUI that previews your real prompt live.
- 🔥 **Hot Reload Customization**: edit `theme.css`, `prompt.html` or `config.txt` and watch the next prompt change. `aswell doctor` validates all of it; broken CSS can never blank your prompt.
- 🌈 **24-bit TrueColor Everywhere**: Builtin `color` command, hex colors (`#ff79c6`), gradients, rainbows, and automatic color flags for everyday CLI tools.
- ⌨️ **Aswell Line Editor (ALE)**: Real-time syntax highlighting, history autosuggestions, fuzzy tab completion, reverse search (`Ctrl+R`), and both Emacs & Vi modes.
- ⚡ **Pure Native Speed**: Zero web engine bloat. Renders directly to ANSI escape codes in microseconds.

---

## 🧩 Customize Everything

| Want to | Do this |
|---|---|
| See what you changed | `aswell config list` |
| Change any setting | `aswell config set <key> <value>` · `aswell config toggle <key>` |
| Read the docs offline | `aswell config help` · `aswell config help <key>` |
| Try a theme first | `aswell theme preview <name>` · `--all` · `--animate` |
| Write your own theme | `aswell theme new mytheme --from nord`, then edit `~/.config/aswell/themes/mytheme.css` |
| Script it / store in dotfiles | `aswell config list --json` · `aswell config export --to aswell.cfg` · `aswell config import aswell.cfg` |
| Use a second config dir | `aswell --config ./aswell-config` or `ASWELL_CONFIG_DIR=…` |
| Check nothing is broken | `aswell doctor` (exit 1 on real problems, `--quiet` for CI) |

Every key above is also a line in `~/.config/aswell/config.txt`, and every one of
them is tab-completed, validated, and hot-applied to your running shell.

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
- [Configuration & Settings Hub](docs/CONFIGURATION.md)
- [CSS & HTML Reference](docs/CSS_REFERENCE.md)
- [POSIX Conformance Matrix](docs/POSIX_COMPLIANCE.md)
- [Plugin System](docs/PLUGINS.md)

---

## 📜 License

MIT License. See [LICENSE](LICENSE) for details.
