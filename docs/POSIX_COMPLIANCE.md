# Aswell POSIX Compliance

Aswell implements the IEEE Std 1003.1 (POSIX) Shell & Utilities specification.

## Implemented POSIX Features

- **Variables & Scoping**: Positional parameters (`$0..$9`, `$*`, `$@`, `$#`), special parameters (`$?`, `$$`, `$!`, `$-`), local variables, exported variables (`export`), readonly variables (`readonly`).
- **Parameter Expansion**: `${VAR}`, `${#VAR}`, `${VAR:-def}`, `${VAR:=def}`, `${VAR:+alt}`, `${VAR:?err}`, `${VAR#pat}`, `${VAR##pat}`, `${VAR%pat}`, `${VAR%%pat}`, `${VAR/pat/repl}`.
- **Quoting & Escaping**: Single quotes `'...'`, double quotes `"..."`, backslash escaping `\`, line continuation `\<newline>`.
- **Command Substitution**: `$(command)` and `` `command` ``.
- **Arithmetic Expansion**: `$(( expression ))` with full C-operator precedence, bitwise operators, power `**`, and ternary `? :`.
- **Tilde Expansion**: `~` and `~user`.
- **Pathname Expansion**: Globbing with `*`, `?`, and `[...]`.
- **Pipelines**: `cmd1 | cmd2 | cmd3` with exit status and `!` pipeline negation.
- **Redirections**: `< file`, `> file`, `>> file`, `>| clobber`, `2>&1`, `<&`, `>&`, `<< EOF` (here-documents), `<<- EOF` (tab stripped here-documents), `<<< "string"` (here-strings).
- **Compound Commands**:
  - `if ... then ... elif ... else ... fi`
  - `for ... in ... do ... done`
  - `while ... do ... done`
  - `until ... do ... done`
  - `case ... in ... esac`
  - Subshells: `( ... )`
  - Groupings: `{ ...; }`
- **Functions**: `name() compound_command` with local scopes, parameter passing, and `return`.
- **Control Flow**: `break [n]`, `continue [n]`, `return [n]`, `exit [n]`.
- **Signals & Traps**: `trap 'commands' SIGNAL...` and `EXIT` pseudo-signal handling.
- **Job Control**: `jobs`, `fg`, `bg`, `wait`, `kill`.
