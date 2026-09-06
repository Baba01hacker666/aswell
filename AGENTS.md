# AGENTS.md — Agent & Contributor Guide for Aswell

Welcome to the **Aswell** codebase! This guide is designed for autonomous AI coding agents (such as Antigravity, Devin, Claude Code, Cursor) and human contributors. It documents the architectural principles, directory structure, build/test workflows, coding invariants, and best practices for developing and maintaining Aswell.

---

## 🧭 Project Overview & Philosophy

**Aswell** is a modern Unix shell built from scratch in **C++20** that bridges two worlds:
1. **Rock-Solid POSIX Compliance**: Complete POSIX.1-2017 standard execution for everyday shell scripts, automation pipelines, and CLI tools.
2. **Next-Generation Interactive UI**: A declarative, HTML/CSS-inspired terminal styling and rendering engine with zero external GUI or web bloat (no Electron, no Chromium, no Node.js, no curses).

### Core Values
- **Zero Web Dependencies**: Everything is rendered directly to VT100/ANSI 24-bit TrueColor terminal escape sequences.
- **Silent & Clean by Default**: The interactive shell behaves like a professional Unix shell (bash, zsh, fish) — clean execution without unsolicited startup banners or post-command execution ribbons unless explicitly configured.
- **Predictable & Robust**: Strict memory safety, comprehensive test suites, and strict compiler enforcement (`-Wall -Wextra -Wpedantic`).

---

## 🏗️ Repository Architecture & Layout

```
aswell/
├── include/aswell/          # Public C++ headers
│   ├── common.hpp           # Common definitions, StringUtils, Terminal helpers
│   ├── shell/               # Core shell subsystem headers
│   │   ├── lexer.hpp        # Tokenizer (POSIX rules)
│   │   ├── parser.hpp       # Recursive-descent AST parser
│   │   ├── ast.hpp          # Abstract Syntax Tree nodes
│   │   ├── expansion.hpp    # 7-stage POSIX word expansion pipeline
│   │   ├── executor.hpp     # Execution engine (fork/exec, pipes, redirections)
│   │   ├── builtins.hpp     # POSIX and Aswell builtins
│   │   ├── environment.hpp  # Variables, PATH, functions, aliases, traps
│   │   ├── signals.hpp      # Signal handlers & terminal state
│   │   └── jobs.hpp         # Job control (fg, bg, jobs, wait)
│   ├── ui/                  # UI & Styling subsystem headers
│   │   ├── dom.hpp          # Lightweight XML/HTML DOM node tree
│   │   ├── css_parser.hpp   # CSS selector matching & cascade engine
│   │   ├── color.hpp        # 24-bit TrueColor, Hex parser, named palettes
│   │   ├── animation.hpp    # Pulse, Rainbow, Wave, Glow, Scramble animations
│   │   ├── layout.hpp       # Multi-column, padding, visual width calculations
│   │   ├── render.hpp       # ANSI sequence generation & TrueColor renderer
│   │   ├── prompt.hpp       # Dynamic prompt engine (prompt.html + theme.css)
│   │   └── terminal.hpp     # Terminal capabilities & raw mode control
│   ├── editor/              # Aswell Line Editor (ALE) headers
│   │   ├── editor.hpp       # Raw terminal input, cursor, multiline editing
│   │   ├── completion.hpp   # Intelligent tab completion & badge classification
│   │   ├── highlighter.hpp  # Real-time syntax coloring
│   │   ├── history.hpp      # History file management & deduplication
│   │   └── suggestions.hpp  # History-based ghost text autosuggestions
│   ├── config/              # Configuration & Themes
│   │   ├── config.hpp       # config.txt parser & settings
│   │   ├── config_editor.hpp# Interactive TUI configuration editor
│   │   └── theme.hpp        # Theme preset loader & palette management
│   └── plugin/              # Extensibility subsystem
│       ├── plugin.hpp       # Dynamic C plugin loader (dlopen/dlsym)
│       └── hooks.hpp        # Lifecycle hooks (on_start, pre_cmd, post_cmd, etc.)
├── src/                     # C++ implementation files matching include/
│   ├── shell/
│   ├── ui/
│   ├── editor/
│   ├── config/
│   ├── plugin/
│   └── main.cpp             # Main shell entry point & interactive REPL
├── tests/                   # Test suite
│   ├── run_all_tests.sh     # Master test runner
│   ├── test_lexer.cpp       # Tokenizer unit tests
│   ├── test_parser.cpp      # AST parser unit tests
│   ├── test_expansion.cpp   # POSIX expansion unit tests
│   ├── test_css.cpp         # CSS parser & cascade tests
│   └── test_ui.cpp          # DOM, animations, TrueColor, and reactive directives
├── examples/                # Example scripts, HTML prompts, and CSS themes
│   ├── cyber_cockpit.html   # Sample multi-line cockpit prompt
│   ├── cyber_scramble.css   # Sample matrix / scramble CSS theme
│   ├── custom_theme.css     # Clean modern CSS theme
│   ├── demo_engine.cpp      # Visual showcase executable for UI & animations
│   └── posix_demo.sh        # POSIX script demonstration
├── docs/                    # Architectural & reference documentation
│   ├── ARCHITECTURE.md      # Detailed layer architecture
│   ├── CONFIGURATION.md     # Configuration file options and format
│   ├── CSS_REFERENCE.md     # Supported CSS selectors and properties
│   ├── PLUGINS.md           # C plugin development guide
│   └── POSIX_COMPLIANCE.md  # Detailed POSIX.1-2017 conformance matrix
└── Makefile                 # Build system
```

---

## ⚡ Build, Run, and Test Workflows

### Building
```bash
# Clean build
make clean && make -j$(nproc)

# Targets built:
#   bin/aswell       - The primary shell executable
#   bin/demo_engine  - The UI/Animation demonstration engine
```

### Running
```bash
# Launch interactive shell
./bin/aswell

# Execute a one-liner command
./bin/aswell -c "echo 'Hello from Aswell'; for x in 1 2 3; do echo item: \$x; done"

# Execute a shell script file
./bin/aswell examples/posix_demo.sh

# Run the dynamic UI and animation showcase
./bin/demo_engine
```

### Testing
Always run the full test suite before committing any changes:
```bash
make test
```
The test suite validates:
1. **Unit Tests**: Lexer, Parser, Expansion, CSS Engine, DOM & Reactive Directives (`show-if`, `hide-if`).
2. **POSIX Script Suite**: Control flow (`if`, `for`, `while`, `until`, `case`), functions, subshells, arithmetic.
3. **Core Semantics**: Pipeline exit codes, trap handlers (`EXIT`, `INT`), subshell copy-on-write isolation.
4. **Animation Engine Stability**: 60fps keyframe tick generators and ANSI sequence consistency.

---

## 🛡️ Critical Agent Invariants & Design Rules

When making code changes to this repository, AI agents **must** adhere to the following non-negotiable rules:

### 1. Compiler Compliance & Code Quality
- **Standard**: Strictly C++20 (`-std=c++20`).
- **Flags**: Must compile with **zero warnings** under `-Wall -Wextra -Wpedantic`. Do not suppress compiler warnings with flags.
- **Dependencies**: Zero third-party UI libraries. Rely exclusively on standard C++ libraries and POSIX system APIs.

### 2. Interactive Terminal Ergonomics
- **No Unsolicited Banners**: The shell must start quietly and execute commands cleanly.
  - Startup banner boxes and post-command execution ribbons are disabled by default.
  - They should only display if explicitly enabled by the user in `~/.config/aswell/config.txt` (`command_banner=true`) or via environment variable.
- **No Default Scramble on Static Elements**:
  - The default theme and prompt must display the real `$USER` username cleanly (e.g. bold cyan/white).
  - Never apply `animation: scramble` to the username or prompt by default. Scramble is an opt-in visual effect for dynamic tickers and badges.

### 3. Custom Commands Subsystem
Aswell provides a dedicated first-class custom commands directory:
- **Location**: `~/.config/aswell/commands/` and `~/.config/aswell/bin/`.
- **PATH Integration**: Automatically prepended to `$PATH` in `Environment::Environment()`.
- **Resolution**: `Environment::find_in_path` discovers commands in this directory even without execution bits or file extensions (`.sh`).
- **Fallback Execution**: `Executor::execute_simple_command` falls back to `/bin/sh` upon `ENOEXEC` or `EACCES` for user custom scripts.
- **Tab Completion**: `CompletionEngine` scans `~/.config/aswell/commands/` and badges custom commands with a bright green `[CUSTOM]` badge in `LineEditor::show_completion_menu`.
- **Management Builtin**: `aswell custom [list | add <name> <script...> | path]`.

### 4. Git & Commit Guidelines
- **Author Identity**: Commits must be authored by:
  `Baba01hacker666 <117832562+Baba01hacker666@users.noreply.github.com>`
- **Commit Format**: Use conventional commits:
  - `feat: ...` for new features
  - `fix: ...` for bug fixes
  - `refactor: ...` for internal restructuring
  - `test: ...` for adding or improving test coverage
  - `docs: ...` for documentation updates

---

## 🧩 Subsystem Architecture Details

### Shell Core (`src/shell/`)
1. **Lexer (`lexer.cpp`)**: Converts UTF-8 stream into tokens. Handles quotes (`'`, `"`), backslash escapes, here-documents (`<<`, `<<-`, `<<<`), and control operators (`&&`, `||`, `|`, `;`, `&`).
2. **Parser (`parser.cpp`)**: Recursive descent parser producing typed AST nodes (`ast.hpp`): `SimpleCommandNode`, `PipelineNode`, `AndOrNode`, `IfNode`, `ForNode`, `WhileNode`, `CaseNode`, `SubshellNode`, `FunctionDefNode`.
3. **Expansion (`expansion.cpp`)**: Implements POSIX 7-stage word expansion:
   - Tilde expansion (`~`, `~user`)
   - Parameter expansion (`$VAR`, `${VAR:-def}`, `${VAR#pat}`, `${#VAR}`, `${VAR/pat/repl}`)
   - Command substitution (`$(...)` and `` `...` ``)
   - Arithmetic expansion (`$(( ... ))` with full operator precedence)
   - Field splitting (`$IFS`)
   - Pathname expansion / Globbing (`*`, `?`, `[...]`)
   - Quote removal
4. **Executor (`executor.cpp`)**: Manages process creation, redirects (`<`, `>`, `>>`, `<&`, `>&`), pipelines with Unix `pipe()` and `fork()`, and job table tracking.
5. **Builtins (`builtins.cpp`)**: Standard builtins (`cd`, `pwd`, `echo`, `printf`, `test`/`[`, `export`, `readonly`, `set`, `unset`, `eval`, `exec`, `read`, `source`, `type`, `kill`, `umask`, `alias`, `unalias`, `exit`, `jobs`, `fg`, `bg`, `wait`, `trap`) plus Aswell control (`aswell config`, `aswell custom`, `aswell theme`, `aswell ui`, `aswell hooks`).

### HTML/CSS Prompt Engine (`src/ui/`)
- **DOM Engine (`dom.cpp`)**: Parses lightweight HTML templates (e.g. `~/.config/aswell/prompt.html`).
  - Available tags: `<prompt>`, `<rprompt>`, `<segment>`, `<user>`, `<hostname>`, `<directory>`, `<git>`, `<runtime>`, `<status>`, `<jobs>`, `<mode>`, `<symbol>`, `<time>`, `<date>`.
  - Reactive directives: `show-if="$STATUS != 0"`, `hide-if="$GIT_BRANCH == ''"`.
  - Variable interpolation: `$VAR` inside text and attributes.
- **CSS Cascade (`css_parser.cpp`)**: Parses standard CSS syntax (`color`, `background-color`, `border`, `padding`, `margin`, `font-weight`, `animation`). Supports element selectors, class selectors (`.pill`), ID selectors (`#cwd`), and pseudo-classes (`:error`, `:dirty`).
- **ANSI & TrueColor Renderer (`render.cpp`, `color.cpp`)**: Translates layout boxes and CSS styles into ANSI escape sequences with precise visual width tracking (correctly ignoring non-printing ANSI escapes and handling UTF-8 wide glyphs).
- **Animations (`animation.cpp`)**: Real-time animations computed from timestamps: `pulse`, `rainbow`, `wave`, `glow`, `scramble`, `fire`, `spin`.

### Line Editor (ALE) (`src/editor/`)
- **Raw Mode Terminal I/O**: Intercepts keypresses using termios raw mode with VT100 escape sequence parsing.
- **Autocomplete (`completion.cpp`)**: Categorized completion menu with typed badges:
  - `[CUSTOM]` (Bright Green) — User scripts in `~/.config/aswell/commands/`
  - `[BUILT ]` (Bright Yellow) — Shell builtins
  - `[ALIAS ]` (Magenta) — Shell aliases
  - `[DIR/  ]` (Blue) — File system directories
  - `[CMD   ]` (Cyan) — Executables in `$PATH`
  - `[$VAR  ]` (Light Cyan) — Environment variables
- **Syntax Highlighting (`highlighter.cpp`)**: Live token coloring as the user types (distinguishes valid commands, arguments, strings, variables, and operators).

---

## 🛠️ How to Perform Common Development Tasks

### Adding a New Builtin Command
1. Declare the method in `include/aswell/shell/builtins.hpp`.
2. Implement the logic in `src/shell/builtins.cpp`.
3. Register the builtin name in `Builtins::is_builtin()` and `Builtins::dispatch()`.
4. Register the builtin in `include/aswell/editor/completion.hpp` and `Builtins::all_builtins()` for tab completion.
5. Add a unit test in `tests/`.

### Adding a New Prompt DOM Element
1. Add the element tag name check in `src/ui/prompt.cpp` within `PromptEngine::resolve_element()`.
2. Implement data retrieval (e.g. system info, battery, network, custom metric).
3. Document the tag in `docs/CONFIGURATION.md` and `README.md`.
4. Add a test case in `tests/test_ui.cpp`.

### Adding a New Animation Type
1. Define the animation enum in `include/aswell/ui/animation.hpp`.
2. Implement the mathematical interpolation in `src/ui/animation.cpp`.
3. Register the CSS animation keyword in `src/ui/css_parser.cpp`.
4. Test in `bin/demo_engine` and `tests/test_ui.cpp`.

---

## 🚀 Quality Checklist for Agents
Before finishing any task or submitting a PR:
- [ ] Code compiles cleanly with `make clean && make -j$(nproc)` with zero warnings.
- [ ] All tests pass via `make test`.
- [ ] No regression in clean terminal behavior (interactive shell starts silently without unwanted banners).
- [ ] Custom commands execute properly from `~/.config/aswell/commands/`.
- [ ] Documentation is updated if user-facing behavior, tags, or options change.
- [ ] Git commit author is set to `Baba01hacker666 <117832562+Baba01hacker666@users.noreply.github.com>`.
