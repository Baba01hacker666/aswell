# Aswell POSIX Compliance

Aswell implements the IEEE Std 1003.1 (POSIX) Shell & Utilities specification.

## Implemented POSIX Features

- **Variables & Scoping**: Positional parameters (`$0..$9`, `$*`, `$@`, `$#`), special parameters (`$?`, `$$`, `$!`, `$-`), local variables, exported variables (`export`), readonly variables (`readonly`).
- **Parameter Expansion**: `${VAR}`, `${#VAR}`, `${VAR:-def}`, `${VAR:=def}`, `${VAR:+alt}`, `${VAR:?err}`, `${VAR#pat}`, `${VAR##pat}`, `${VAR%pat}`, `${VAR%%pat}`, `${VAR/pat/repl}`.
- **Quoting & Escaping**: Single quotes `'...'`, double quotes `"..."`, backslash escaping `\`, line continuation `\<newline>`.
- **Command Substitution**: `$(command)` and `` `command` ``.
- **Arithmetic Expansion**: `$(( expression ))` with full C-operator precedence, bitwise operators, power `**`, and ternary `? :`.
- **Word Expansion (Stage 0-6 Pipeline)**:
  - **Brace Expansion**: Comma separation `{a,b,c}`, numeric ranges `{1..10}` and `{10..1}`, zero-padded ranges `{01..05}`, step increments `{1..10..2}`, character ranges `{a..z}`, nested braces `a{b,c{1,2}}d`, and Cartesian products `{1,2}_{3,4}`.
  - **Tilde Expansion**: `~` and `~user`.
  - **Parameter Expansion**: `${VAR}`, `${#VAR}`, `${VAR:-def}`, `${VAR:=def}`, `${VAR:+alt}`, `${VAR:?err}`, `${VAR#pat}`, `${VAR##pat}`, `${VAR%pat}`, `${VAR%%pat}`, `${VAR/pat/repl}`.
  - **Command Substitution**: `$(command)` and `` `command` ``.
  - **Arithmetic Expansion**: `$(( expression ))` with full C-operator precedence, bitwise operators, power `**`, and ternary `? :`.
  - **Field Splitting**: Respects `$IFS`.
  - **Pathname Expansion**: Globbing with `*`, `?`, and `[...]`.
  - **Quote Removal**: Single and double quotes removed in final stage.
- **Interactive History Expansion**: `!!`, `!$`, `!^`, `!*`, `!-n`, `!n`, `!prefix`, `!?query?`, and word modifiers `:$`, `:^`, `:*`.
- **Directory Stack Subsystem**: `pushd`, `popd`, `dirs` with `-c` (clear), `-v` (indexed display), `-p` (per-line), `+N` (rotation and indexed removal).
- **Builtin Utilities**:
  - POSIX standard: `command` (`-p`, `-v`, `-V`), `cd`, `pwd`, `echo`, `printf`, `test`/`[`, `export`, `readonly`, `set`, `unset`, `eval`, `exec`, `read`, `source` / `.`, `type`, `kill`, `umask`, `alias`, `unalias`, `exit`, `history` (`-c`, `-d <offset>`, `<count>`).
  - Job control: `jobs`, `fg`, `bg`, `wait`.
  - Enhanced styling: `color` (TrueColor, 256-color, named palettes, gradients, rainbow, streaming stdin pipelines).
