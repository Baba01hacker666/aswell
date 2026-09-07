<p align="center">
  <h1 align="center">⚡ Aswell</h1>
  <p align="center">
    <strong>An Extremely Customizable, High-Performance POSIX-Compatible Unix Shell</strong>
  </p>
  <p align="center">
    Built from scratch in C++20 — Zero Electron, Zero Chromium, Zero Web Bloat.
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

## 🌟 Overview

**Aswell** is a modern Unix shell built from scratch in C++20 that bridges two worlds:
1. **Rock-solid POSIX standard execution**: Runs everyday shell scripts, automation pipelines, and CLI tools with complete POSIX.1-2017 compatibility.
2. **Next-generation interactive customization**: A declarative, HTML/CSS-inspired UI and layout engine right inside your terminal.

Customize your prompt, status indicators, and alerts using the styling concepts you already know — colors (Hex/RGB/TrueColor), borders, padding, margins, flex-style tags, and 60fps animations — without sacrificing shell speed or POSIX script compatibility.

---

## ✨ Features

### 🐚 Complete POSIX.1-2017 Compatibility
- **Full Parameter Expansion**:
  - Value expansion: `$VAR`, `${VAR}`
  - String length: `${#VAR}`
  - Default values: `${VAR:-default}`, `${VAR:=default}`, `${VAR:?error}`, `${VAR:+alternative}`
  - Substring pattern removal: `${VAR#prefix}`, `${VAR##prefix}`, `${VAR%suffix}`, `${VAR%%suffix}`
  - Pattern substitution: `${VAR/search/replace}`, `${VAR//search/replace}`
- **Quoting & Escaping**: Strict single quoting `'...'`, double quoting `"..."` with expansion, and backslash `\` escapes.
- **Command & Arithmetic Substitution**:
  - `$(command)` and legacy backticks `` `command` ``
  - `$(( expression ))` with full C-operator precedence: addition, multiplication, division, modulo, bitwise shifts, comparisons, ternary `? :`, and exponentiation `**`.
- **Pipelines & Redirections**:
  - Multi-stage pipelines: `cmd1 | cmd2 | cmd3` with negation (`!`) and pipeline exit status preservation.
  - File redirection: `< input.txt`, `> out.txt`, `>> append.txt`, `>& target`, `2>&1`.
  - Heredocs and Here-strings: `<< EOF`, `<<- EOF` (tabs stripped), `<<< "inline string"`.
- **Control Flow & Scripting**:
  - Conditionals: `if ...; then ...; elif ...; else ...; fi`
  - Loops: `for var in words; do ...; done`, `while condition; do ...; done`, `until condition; do ...; done`
  - Pattern matching: `case word in pattern) ... ;; esac`
  - Function declarations and invocations with isolated local argument lists (`$1`, `$@`, `$#`).
- **Process & Job Control**:
  - Subshells `( ... )` with copy-on-write process isolation and grouping `{ ...; }`.
  - Asynchronous background jobs (`&`), job listings (`jobs`), foregrounding (`fg`), backgrounding (`bg`), and job reaping (`wait`).
  - Standard signals (`SIGINT`, `SIGTERM`, `SIGWINCH`, `SIGCHLD`) and signal traps (`trap`).
- **Standard Builtin Suite**:
  `cd`, `pwd`, `dirs`, `pushd`, `popd`, `command`, `echo`, `printf`, `color`, `test`/`[`, `export`, `readonly`, `set`, `unset`, `eval`, `exec`, `read`, `source` / `.`, `type`, `kill`, `umask`, `alias`, `unalias`, `exit`, `history`, `true`, `false`, `aswell`.
- **Advanced Expansion Pipeline**:
  Full 7-stage POSIX expansion with **Brace Expansion** (`{a,b,c}`, `{1..10}`, `{01..05}`, `{a..z}`, nested braces, Cartesian products) and **Interactive History Expansion** (`!!`, `!$`, `!^`, `!*`, `!-n`, `!n`, `!prefix`, `!?str`).

---

### 🎨 HTML & CSS Prompt Styling Engine
No more unreadable esoteric bash escape codes like `\[\033[01;32m\]\u@\h\[\033[00m\]`. Aswell introduces semantic UI tags and clean CSS:

#### Semantic DOM Elements
```html
<prompt>
  <user />
  <hostname />
  <directory />
  <git />
  <runtime />
  <status />
  <jobs />
  <mode />
  <symbol />
</prompt>
```

#### CSS Styling
```css
prompt {
  display: block;
}

user {
  color: #ff79c6;
  font-weight: bold;
}

directory {
  color: #8be9fd;
  border-left: 2px solid #50fa7b;
  padding: 0 1ch;
}

git {
  color: #f1fa8c;
  background-color: #282a36;
}

symbol {
  color: #bd93f9;
  animation: pulse 1s infinite;
}
```
- **Supported CSS Properties**: `color`, `background-color`, `font-weight` (bold/normal), `font-style` (italic/normal), `text-decoration` (underline), `border-left`, `border-right`, `padding`, `margin`, `display`, `animation`.
- **Zero Webview Engine**: Handcrafted zero-allocation tokenizing parser written in C++20. Ultra-lightweight and lightning fast.

---

### 🌀 Native Animation Engine
Aswell provides smooth, non-blocking terminal animations powered by an asynchronous tick loop:
- **`pulse`**: Smoothly modulates alpha/luminance.
- **`rainbow`**: Real-time HSV hue rotation across TrueColor spectrum.
- **`fire`**: Dynamic organic flame gradient oscillation.
- **`spin`**: Rotating glyphs (`|`, `/`, `-`, `\`).
- **`wave`**: Sinusoidal color oscillation across text spans.
- *Graceful Degradation*: Automatically disables animations when running non-interactively or in dumb terminals (`TERM=dumb`).

---

### ⌨️ Aswell Line Editor (ALE)
A dedicated, fully custom interactive line editor designed for developer productivity:
- **Real-time Syntax Highlighting**: Instant visual feedback for commands, builtins, options/flags, quoted strings, variables, and operators as you type.
- **History Autosuggestions**: Ghost text suggestions drawn from command history; press `Right Arrow` or `End` to accept.
- **Fuzzy Tab Completion**: Context-aware completion for executables in `$PATH`, built-in commands, file system paths, environment variables, and Git branches.
- **Reverse History Search (`Ctrl+R`)**: Live substring and incremental history search.
- **Multiline Input**: Smart continuation for open quotes, unclosed parenthesis, or trailing backslashes with customizable continuation prompt (`> `).
- **Dual Modal Editing**: Supports both standard **Emacs** and **Vi** keybindings.

---

### 🎭 Themes & Interactive TUI Configurator

#### Built-in Themes
Choose from 6 professionally crafted themes out of the box:
- `modern` (Default clean theme with subtle status cards)
- `cyberpunk` (High-contrast neon cyan, yellow, and magenta)
- `nord` (Arctic, elegant pastel blues and snow-white)
- `minimal` (Distraction-free, monochrome simplicity)
- `dracula` (Classic dark theme with vibrant accents)
- `powerline` (Segmented glyphs and solid backgrounds)

Switch themes on the fly:
```bash
aswell theme list
aswell theme set nord
```

#### Interactive Configuration UI
Launch the interactive configuration TUI anytime:
```bash
aswell config
```
Toggle prompt modules (Git, Status, Runtime, Jobs), choose active themes, and preview your prompt in real-time.

---

### 🌈 Colored Outputs & Styling in Commands
Aswell brings first-class color output support directly to commands and scripts:
- **`color` Builtin & `aswell color` Command**:
  Easily output formatted and styled text without memorizing raw ANSI escape codes:
  ```bash
  # Named colors and 24-bit TrueColor hex
  color red "Error: connection timed out"
  color green --bold "Build succeeded!"
  color --bg "#222222" "#00f0ff" "Cyberpunk neon text"

  # Smooth gradients and rainbow spectrums
  color gradient cyan magenta "Beautiful flowing status"
  color rainbow "Vibrant rainbow text"

  # Pipeline streaming from commands
  cat app.log | color cyan
  ls | color green

  # Inspect available color palettes
  color list
  ```
- **Enhanced `echo` and `printf` Escapes**:
  Complete support for standard octal escapes (`\033`), hex escapes (`\x1b`), `\e`, and `%b` in `printf` and `echo -e`:
  ```bash
  echo -e "\033[31mRed Alert\033[0m"
  printf "\033[1;32m%s\033[0m\n" "Success"
  printf "%b\n" "\033[34mBlue via %b\033[0m"
  ```
- **Automatic Color for Interactive CLI Commands**:
  Interactive shells export `COLORTERM=truecolor` and `CLICOLOR=1` and enable default color aliases (`ls --color=auto`, `grep --color=auto`, `diff --color=auto`, etc.) by default. Can be toggled in `aswell config` or `~/.config/aswell/config.txt` (`colored_output=true`).

---

### 💡 Intelligent Diagnostics & Error Cards
Interactive typos are automatically caught with helpful suggestions and suggestions:
```
╭─ Error: Command Not Found ──────────────────────────────────╮
│ 'gti' is not recognized as a command or builtin.            │
│ Did you mean:                                               │
│   • git                                                     │
╰─────────────────────────────────────────────────────────────╯
```
*(In non-interactive script mode, standard POSIX error output is strictly maintained).*

---

## 🚀 Quick Start

## 🚀 Installation

### One-Line Install (Prebuilt Binaries)

Install prebuilt binaries for Linux (`x86_64` and `aarch64`) or **Android (Termux)** directly via `curl` with zero build dependencies:

```bash
curl -fsSL https://raw.githubusercontent.com/Baba01hacker666/aswell/master/install.sh | bash
```

> **📱 Termux on Android**: Run `pkg update && pkg install curl tar`, then run the command above. The installer automatically detects Termux, installs to `$PREFIX/bin/aswell`, configures `$PREFIX/etc/shells`, and sets up fallback execution via `/data/data/com.termux/files/usr/bin/bash`.

This installer fetches pre-compiled release binaries built by GitHub Actions workflows, installs `aswell` and `aswell-demo` directly to `/usr/local/bin` (or `$PREFIX/bin` / `~/.local/bin`), initializes custom commands and themes in `~/.config/aswell/`, and registers the shell.

---

### Building from Source (Optional)

If you prefer building from source:

#### Prerequisites
- Linux with GCC 10+ or Clang 11+ (C++20 support)
- GNU Make

```bash
# Clone the repository
git clone https://github.com/Baba01hacker666/aswell.git
cd aswell

# Compile the shell binary
make -j$(nproc)

# Install system-wide (installs to /usr/local/bin/aswell)
sudo make install
```

### Running Aswell

```bash
# Start an interactive session
aswell

# Execute a one-liner command
aswell -c 'echo "Current shell: $0"; for x in 1 2 3; do printf "%d " "$((x * 2))"; done; echo'

# Run a POSIX shell script
aswell path/to/script.sh

# Use in script shebangs:
#!/usr/bin/env aswell
```

---

## 🧪 Test Suite

Aswell features an automated test harness covering lexer tokenization, AST parser construction, expansion stages, CSS cascading, and POSIX shell semantics:

```bash
./tests/run_all_tests.sh
```

Tests include:
- `test_lexer`: Token types, quoting boundaries, escapes, heredoc delimiters.
- `test_parser`: Pipeline trees, subshell nodes, conditional branches, loops.
- `test_expansion`: Variable interpolation, substring manipulation, globbing, arithmetic.
- `test_css`: Selector resolution, hex/RGB color parsing, style property cascades.
- `test_ui`: DOM construction, ANSI TrueColor escape sequence rendering.
- `posix_demo.sh` & `loops_and_functions.sh`: Real script execution, loops, function local variables, exit code traps.

---

## 📂 Project Architecture

```
aswell/
├── bin/                       # Output binaries
├── docs/                      # Technical documentation
│   ├── ARCHITECTURE.md        # System architecture and design docs
│   ├── CONFIGURATION.md       # Configuration and .aswellrc guide
│   ├── CSS_REFERENCE.md       # Complete CSS and HTML tag specification
│   ├── PLUGINS.md             # Plugin architecture & hook hooks
│   └── POSIX_COMPLIANCE.md    # POSIX.1-2017 compliance status
├── examples/                  # Sample scripts, themes, and configs
├── include/aswell/            # C++20 Header files
│   ├── common.hpp             # Shared types, string utilities
│   ├── config/                # Configuration and theme models
│   ├── editor/                # Line editor, syntax highlighter, completions
│   ├── plugin/                # Hook subsystem and script plugins
│   ├── shell/                 # Lexer, Parser, AST, Expansion, Executor, Builtins
│   └── ui/                    # DOM, CSS engine, Terminal TrueColor, Animations
├── plugins/                   # Built-in hooks and shell plugins
├── src/                       # C++20 Implementations
│   ├── config/
│   ├── editor/
│   ├── plugin/
│   ├── shell/
│   ├── ui/
│   └── main.cpp               # CLI entrypoint and REPL driver
├── tests/                     # Unit and integration test suites
└── themes/                    # CSS prompt themes
```

---

## 📖 Documentation

- [Architecture Overview](docs/ARCHITECTURE.md)
- [Configuration Guide](docs/CONFIGURATION.md)
- [CSS & HTML Customization Reference](docs/CSS_REFERENCE.md)
- [Plugin System](docs/PLUGINS.md)
- [POSIX Compliance Matrix](docs/POSIX_COMPLIANCE.md)

---

## 📜 License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.
