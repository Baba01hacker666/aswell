# Aswell Architecture

Aswell is an extremely customizable, modern Unix shell designed from the ground up to combine rigorous POSIX compatibility with a native, declarative HTML/CSS-inspired terminal rendering engine.

## Architectural Layers

```
┌─────────────────────────────────────────────────────────┐
│                 Interactive Terminal UI                 │
├─────────────────────────────────────────────────────────┤
│        Customization & Layout Engine (DOM + CSS)        │
├─────────────────────────────────────────────────────────┤
│              Animation & TrueColor Subsystem            │
├─────────────────────────────────────────────────────────┤
│               Line Editor (ALE) & Highlighting          │
├─────────────────────────────────────────────────────────┤
│                     Plugin Platform                     │
├─────────────────────────────────────────────────────────┤
│                Shell Parser & AST Generator             │
├─────────────────────────────────────────────────────────┤
│                      Expansion Engine                   │
├─────────────────────────────────────────────────────────┤
│                      Execution Engine                   │
├─────────────────────────────────────────────────────────┤
│                   Job & Signal Management               │
├─────────────────────────────────────────────────────────┤
│                   POSIX Compatibility Core              │
├─────────────────────────────────────────────────────────┤
│                      Unix / Linux OS                    │
└─────────────────────────────────────────────────────────┘
```

## Layer Breakdown

### 1. POSIX Compatibility & Execution Engine (`src/shell/`)
- **Lexer (`lexer.cpp`)**: Tokenizes input following IEEE Std 1003.1, handling single quotes, double quotes, escape sequences, line continuations, and here-document collection.
- **Parser (`parser.cpp`)**: Recursive descent AST parser converting tokens into abstract syntax trees for simple commands, pipelines, and-or lists, subshells, loops (`for`, `while`, `until`), conditionals (`if`, `elif`, `else`), `case` statements, and function definitions.
- **Expansion (`expansion.cpp`)**: Implements the POSIX 7-stage word expansion pipeline:
  1. Tilde expansion (`~`, `~user`)
  2. Parameter expansion (`$VAR`, `${VAR#pat}`, `${VAR%pat}`, `${VAR:-default}`, `${VAR:=default}`, `${#VAR}`, etc.)
  3. Command substitution (`$(cmd)` and `` `cmd` ``)
  4. Arithmetic expansion (`$((expr))` with C-style operator precedence)
  5. Field splitting (`$IFS`)
  6. Pathname expansion (globbing with `*`, `?`, `[...]`)
  7. Quote removal
- **Executor (`executor.cpp`)**: Evaluates AST nodes, setups file descriptors and redirections (`<`, `>`, `>>`, `<&`, `>&`, `<<`, `<<<`), spawns pipelines with Unix `pipe()` and `fork()`, and orchestrates process execution.
- **Builtins (`builtins.cpp`)**: Pure native implementation of standard POSIX shell builtins (`cd`, `pwd`, `echo`, `printf`, `test`/`[`, `exit`, `set`, `unset`, `export`, `readonly`, `alias`, `unalias`, `eval`, `exec`, `read`, `source`, `shift`, `trap`, `type`, `wait`, `jobs`, `fg`, `bg`, `kill`, `hash`, `umask`, `local`, `break`, `continue`, `return`).
- **Job & Signal Control (`jobs.cpp`, `signals.cpp`)**: Terminal process group management via `tcsetpgrp()`, foreground/background task switching, and signal traps.

### 2. Customization & Styling Engine (`src/ui/`)
- **DOM Engine (`dom.cpp`)**: Lightweight declarative tag tree (`<prompt>`, `<segment>`, `<user>`, `<hostname>`, `<directory>`, `<git>`, `<runtime>`, `<status>`, `<symbol>`).
- **CSS Engine (`css_parser.cpp`)**: Parses CSS properties, selector matching (tag, class `.name`, id `#name`, pseudo `:error`, `:dirty`), cascade resolution, and box model styles.
- **Color Engine (`color.cpp`)**: 24-bit TrueColor ANSI conversion, hex parser (`#00f0ff`), named palette mappings (Nord, Cyberpunk, Dracula), and gradient blending.
- **Animation Framework (`animation.cpp`)**: Time-based keyframe calculations for `pulse`, `rainbow`, `fire`, `spin`, and `wave`. Safe fallback to static rendering in non-interactive terminals.
- **Layout & Renderer (`layout.cpp`, `render.cpp`)**: Calculates element visual widths (accounting for multi-byte UTF-8, zero-width ANSI codes, and wide characters) and emits escape sequences.

### 3. Aswell Line Editor (ALE) (`src/editor/`)
- **Editor (`editor.cpp`)**: Raw mode terminal I/O, cursor navigation, history navigation, multiline editing with continuation prompts, Emacs & Vi modes.
- **Syntax Highlighter (`highlighter.cpp`)**: Real-time syntax coloring of commands, builtins, keywords, options, strings, variables, and operators as the user types.
- **AutoSuggestions (`suggestions.cpp`)**: Prefix matching against history with inline ghost text.
- **Completion Engine (`completion.cpp`)**: Intelligent tab completion with fuzzy subsequence matching for executables in `$PATH`, builtins, aliases, files, directories, and variables.
