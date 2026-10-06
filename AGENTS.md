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
│   │   ├── parallel.hpp     # Pure helpers of the `parallel` builtin (testable)
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
│   │   ├── config.hpp       # config.txt parser, ShellConfig, ConfigWatcher, reports
│   │   ├── settings.hpp     # Declarative settings registry (single source of truth)
│   │   ├── settings_cli.hpp # `aswell config|theme|doctor|aliases|reload` customization hub
│   │   ├── alias_library.hpp# Curated alias/function table (58 entries, 9 categories)
│   │   ├── config_editor.hpp# Interactive TUI configuration editor (generated from the registry)
│   │   └── theme.hpp        # Theme presets, user themes, resolution order
│   └── plugin/              # Extensibility subsystem
│       ├── plugin.hpp       # Dynamic C plugin loader (dlopen/dlsym)
│       └── hooks.hpp        # Lifecycle hooks (on_start, pre_cmd, post_cmd, etc.)
├── src/                     # C++ implementation files matching include/
│   ├── shell/
│   │   └── builtins_parallel.cpp # `parallel`, `retry`, `timeout` (fork, poll, signals)
│   ├── ui/
│   ├── editor/
│   ├── config/
│   │   ├── settings.cpp     # The registry table: one row per user-facing knob
│   │   ├── settings_cli.cpp # Settings hub: config/theme/aliases/doctor/reload commands
│   │   ├── alias_library.cpp# The curated alias table + install/uninstall/file handling
│   │   └── ...
│   ├── plugin/
│   └── main.cpp             # Main shell entry point, interactive REPL & apply_config()
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
5. **Customization Hub** (`tests/test_config.cpp` + suite section 8): settings
   registry lookups/aliases/validation, `config.txt` parse + round-trip, comment
   preserving writes, `ConfigWatcher` hot-reload detection, theme resolution and
   scaffolding, hub Tab completion, JSON output; end-to-end `aswell config`,
   `aswell theme`, `aswell doctor` and `aswell reload` behaviour in a scratch `$HOME`.
6. **Parallelism & Alias Library** (`tests/test_features.cpp` + suite section 9):
   template substitution, stdin/`:::` job lists, keep-order, `-e`/`-T`/dry-run,
   `retry` backoff and `-s` accepted statuses, `timeout` exit codes (124/137),
   job specs, curated-alias table invariants, install/uninstall round-trips against
   a temp `$ASWELL_CONFIG_DIR`, and that installed aliases work inside `parallel` jobs.

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

### 2b. Customization Ergonomics
- **One path for one action**: `aswell config …` / `aswell theme …` behave
  identically as a builtin and as `aswell config …` on the CLI (both go through
  `SettingsCli`). Do not duplicate theme or settings logic in `src/main.cpp`.
- **Never store a setting in only one place**: writing `ASWELL_THEME` (or any
  env var) is not persistence — persist through `ConfigManager::update_keys()`
  and let `Environment::config_reload` apply it live.
- **Fail loudly, then help**: unknown keys/typos must produce a suggestion
  (`SettingsRegistry::suggest`) and a pasteable fix, both at startup and from
  `aswell doctor`. A user editing `theme.css`/`prompt.html` sees the result on
  the next prompt (`auto_reload`, default on) — keep that guarantee.

### 3. Custom Commands Subsystem
Aswell provides a dedicated first-class custom commands directory:
- **Location**: `~/.config/aswell/commands/` and `~/.config/aswell/bin/`.
- **PATH Integration**: Automatically prepended to `$PATH` in `Environment::Environment()`.
- **Resolution**: `Environment::find_in_path` discovers commands in this directory even without execution bits or file extensions (`.sh`).
- **Fallback Execution**: `Executor::execute_simple_command` falls back to `/bin/sh` upon `ENOEXEC` or `EACCES` for user custom scripts.
- **Tab Completion**: `CompletionEngine` scans `~/.config/aswell/commands/` and badges custom commands with a bright green `[CUSTOM]` badge in `LineEditor::show_completion_menu`.
- **Management Builtin**: `aswell custom [list | add <name> <script...> | path]`.

### 4. Colored Outputs & Commands Subsystem
Aswell provides dedicated primitives and environment variables for rich terminal colors:
- **Builtin `color` Command & `aswell color`**: Supports named colors, 24-bit TrueColor (`#rrggbb`), gradients (`color gradient <c1> <c2> <text>`), rainbow effects (`color rainbow <text>`), style flags (`--bold`, `--dim`, `--italic`, `--underline`, `--reverse`, `--bg`), template evaluation (`color eval <markup>`), and streaming pipelines via stdin (`cmd | color <c>`).
- **Full Escape Sequence Conformance**: `echo -e` and `printf` support octal escapes (`\033`), hex escapes (`\x1b`), `\e`, and `%b` (POSIX argument escape expansion).
- **Environment & Aliases**: Exports `COLORTERM=truecolor` and `CLICOLOR=1` and sets default color aliases (`ls`, `grep`, `diff`, etc.) in interactive mode, toggleable via `colored_output=true` in `~/.config/aswell/config.txt`.

### 5. Low-Footprint (`anon`) Profile
`--anon` (or `ASWELL_ANON=1`, or `anon_mode=true`) starts a deliberately quiet,
stateless shell for locked-down machines: no history file, no config writes, no
plugin autoload, no `~/.bashrc` import, no animations, and — for a program that is
a single external command — `exec` in place instead of `fork`. It is a footprint
reduction that is always visible in `aswell doctor`; it must never become a way to
hide activity from an administrator. Adding a setting means adding the registry row
*and* reading it in `apply_config()` (`src/main.cpp`), like any other knob.

### 6. Git & Commit Guidelines
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
5. **Builtins (`builtins*.cpp`)**: Standard builtins (`cd`, `pwd`, `echo`, `printf`, `test`/`[`, `export`, `readonly`, `set`, `unset`, `eval`, `exec`, `read`, `source`, `type`, `kill`, `umask`, `alias`, `unalias`, `local`, `exit`, `jobs`, `fg`, `bg`, `wait`, `trap`) plus Aswell-only ones (`parallel`, `retry`, `timeout`) and shell control (`aswell config`, `aswell aliases`, `aswell custom`, `aswell theme`, `aswell ui`, `aswell hooks`). Job specs (`%1`, `%+`, `%-`, `%%`, `%?cmd`) resolve through `JobManager::resolve`, shared by `jobs`/`fg`/`bg`/`wait`/`kill`.
6. **Parser (`parser.cpp`)**: reserved words are reserved only at the *start* of a command; `collect_simple_command_words()` accepts `done`/`fi`/`then`/… as arguments (as bash does) while keeping `esac` reserved so malformed `case` blocks still error.

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
2. Implement the logic in the matching `src/shell/builtins_*.cpp` (large features get
   their own file, e.g. `builtins_parallel.cpp` for `parallel`/`retry`/`timeout`).
3. Register the builtin name in `Builtins::is_builtin()` **and** `Builtins::dispatch()` —
   both lists must agree or the command is half-known.
4. Add it to the builtin table in `src/editor/completion.cpp` (menu + badges) and to
   the name list in `Executor::find_similar_commands` (`src/shell/executor.cpp`) so
   `type` and "did you mean" know it.
5. Document it in `docs/CONFIGURATION.md` and in `aswell help`
   (`src/shell/builtins_io.cpp`) — help text is generated from the same facts.
6. Add a unit test in `tests/` plus an end-to-end check in `tests/run_all_tests.sh`.
7. Anything that forks must give the child its own process group (`setpgid`) and must
   restore signals; anything that pumps output must use `O_NONBLOCK` on the read end,
   otherwise "parallel" output silently serialises (see `builtins_parallel.cpp`).

### Adding a New Prompt DOM Element
1. Add the element tag name check in `src/ui/prompt.cpp` within `PromptEngine::resolve_element()`.
2. Implement data retrieval (e.g. system info, battery, network, custom metric).
3. Document the tag in `docs/CONFIGURATION.md` and `README.md`.
4. Add a test case in `tests/test_ui.cpp`.

### Adding a New Setting (the pattern to follow)
Settings are **declarative**: `include/aswell/config/settings.hpp` +
`src/config/settings.cpp` describe every knob once, and `config.txt` parsing,
serialization, `aswell config list/get/set/toggle/unset/help/export/import`,
the TUI editor, `aswell doctor` and Tab completion all derive from that table.

1. Add the field with its default to `ShellConfig` (`include/aswell/config/config.hpp`).
2. Add one row in `registry()` using a factory:
   `bool_setting("my_flag", {"alias"}, "appearance", "Description", &ShellConfig::my_flag)`
   (also available: `string_setting`, `enum_setting`, `int_setting`).
3. **Make it real**: read it in `apply_config()` in `src/main.cpp` (the single
   place that pushes settings into `PromptEngine`, `LineEditor`, `History`, …) so
   it applies at startup, on `aswell reload`, on hot reload and after
   `aswell config set` — never read config.txt ad hoc somewhere else.
4. Add/extend a case in `tests/test_config.cpp` and mention it in
   `docs/CONFIGURATION.md` (the table there mirrors the registry).

Never hard-code a list of themes, settings or CSS properties elsewhere; ask
`ThemeManager::theme_names()`, `SettingsRegistry::all()` or
`CSSParser::supported_properties()`. `aswell doctor` validates user files
against the same lists, which is what keeps docs, code and UX from drifting.

### Adding a Curated Alias or Helper Function
The alias library is declarative in exactly the same way settings are: one row in
`src/config/alias_library.cpp` feeds `aswell aliases list|show|search|preview|install`,
the `curated_aliases` setting applied at startup, `~/.config/aswell/aliases`, and
`aswell doctor`.

1. Add a row: `{name, AliasKind::ALIAS|FUNCTION, category, definition, description,
   needs, shadows}`. Reuse an existing category unless a new group genuinely helps.
2. `needs` names a binary the entry requires (`"docker"`, `"fd"`): rows whose tool is
   missing are skipped with a reason instead of installing something broken.
3. `shadows` marks entries that replace a real command (only the `safe` category may
   do that) so the installer warns rather than overriding silently.
4. Keep definitions single-line for aliases; function rows are full shell source and
   are installed by *executing* them (`executor->execute_string`), which defines the
   function without running it. They may use `local`, `${VAR%pattern}` and word
   splitting, but must stay parseable by this shell — `tests/test_features.cpp`
   asserts every rendered row parses and defines its name.
5. `tests/test_features.cpp` also enforces unique names, non-empty description and
   definition, and a known category, so a new row is validated without writing a test.

### Adding a New Animation Type
1. Define the animation enum in `include/aswell/ui/animation.hpp`.
2. Implement the mathematical interpolation in `src/ui/animation.cpp`.
3. Register the CSS animation keyword in `src/ui/css_parser.cpp`
   (`pulse`, `rainbow`, `fire`, `spin`, `wave`, `scramble`/`glitch`/`matrix`) and
   add the name to `CSSParser::supported_properties()`-adjacent docs.
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
