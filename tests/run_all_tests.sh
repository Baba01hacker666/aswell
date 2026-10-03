#!/bin/bash
set -e

echo "=================================================="
echo "          ASWELL TEST SUITE EXECUTION             "
echo "=================================================="

BUILD_DIR="bin"
mkdir -p "$BUILD_DIR"

# Removes ANSI/SGR sequences so behaviour assertions can match on plain text.
strip_ansi() { sed -e "s/$(printf '\033')\[[0-9;]*m//g"; }

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -O2}"
LDFLAGS="${LDFLAGS:-}"

echo "1. Building Unit Tests..."
$CXX $CXXFLAGS -Iinclude tests/test_lexer.cpp src/shell/lexer.o -o bin/test_lexer $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_parser.cpp src/shell/lexer.o src/shell/parser.o -o bin/test_parser $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_expansion.cpp src/shell/expansion.o src/shell/environment.o src/shell/signals.o -o bin/test_expansion $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_css.cpp src/ui/css_parser.o src/ui/color.o -o bin/test_css $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_ui.cpp src/ui/dom.o src/ui/layout.o src/ui/render.o src/ui/color.o src/ui/animation.o src/ui/css_parser.o src/ui/prompt.o src/ui/terminal.o src/ui/template_engine.o src/shell/environment.o -o bin/test_ui $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_history.cpp src/editor/history.o -o bin/test_history $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_bashcompat.cpp src/shell/*.o src/editor/*.o src/ui/*.o src/config/*.o -o bin/test_bashcompat $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_completion.cpp src/shell/*.o src/editor/*.o src/ui/*.o src/config/*.o -o bin/test_completion $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_cd_hash_umask.cpp src/shell/*.o src/editor/*.o src/ui/*.o src/config/*.o -o bin/test_cd_hash_umask $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_getopts.cpp src/shell/*.o src/editor/*.o src/ui/*.o src/config/*.o -o bin/test_getopts $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_stty.cpp src/shell/*.o src/editor/*.o src/ui/*.o src/config/*.o -o bin/test_stty $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_config.cpp src/shell/*.o src/editor/*.o src/ui/*.o src/config/*.o src/plugin/*.o -o bin/test_config $LDFLAGS

echo "2. Running Unit Tests..."
./bin/test_lexer
./bin/test_parser
./bin/test_expansion
./bin/test_css
./bin/test_ui
./bin/test_history
./bin/test_bashcompat
./bin/test_completion
./bin/test_cd_hash_umask
./bin/test_getopts
./bin/test_stty
./bin/test_config

echo ""
echo "3. Running POSIX Compatibility Script Suite..."
./bin/aswell examples/posix_demo.sh > /tmp/posix_demo_out.txt
grep -q "Shell: Aswell v1.0.0" /tmp/posix_demo_out.txt
grep -q "Basename using % strip: script.sh" /tmp/posix_demo_out.txt
grep -q "Arithmetic 11" /tmp/posix_demo_out.txt
grep -q "Demo completed successfully!" /tmp/posix_demo_out.txt
echo "[PASS] posix_demo.sh"

./bin/aswell examples/loops_and_functions.sh > /tmp/loops_out.txt
grep -q "While iteration: 3" /tmp/loops_out.txt
grep -q "Fruit: banana" /tmp/loops_out.txt
grep -q "Matched cherry!" /tmp/loops_out.txt
echo "[PASS] loops_and_functions.sh"

./bin/aswell examples/arithmetic_test.sh > /tmp/arith_out.txt
grep -q "Z = 50" /tmp/arith_out.txt
grep -q "Bitwise: 20" /tmp/arith_out.txt
grep -q "Comparison result: 1" /tmp/arith_out.txt
echo "[PASS] arithmetic_test.sh"

echo ""
echo "4. Running POSIX Core Semantics Tests..."

# Test exit code propagation
set +e
./bin/aswell -c 'exit 37'
STATUS=$?
set -e
if [ "$STATUS" -ne 37 ]; then
    echo "[FAIL] Expected exit code 37, got $STATUS"
    exit 1
fi
echo "[PASS] Exit code propagation (status 37)"

# Test pipelines with exit code
OUT=$(./bin/aswell -c 'printf "cat\ndog\nfish\n" | grep dog')
if [ "$OUT" != "dog" ]; then
    echo "[FAIL] Pipeline output mismatch: $OUT"
    exit 1
fi
echo "[PASS] Pipeline execution"

# Test subshell isolation
OUT=$(./bin/aswell -c 'A=1; (A=2; echo -n "$A,"); echo "$A"')
if [ "$OUT" != "2,1" ]; then
    echo "[FAIL] Subshell isolation mismatch: $OUT"
    exit 1
fi
echo "[PASS] Subshell environment isolation"

# Test function scope and return
OUT=$(./bin/aswell -c 'calc() { return 42; }; calc; echo $?')
if [ "$OUT" != "42" ]; then
    echo "[FAIL] Function return status mismatch: $OUT"
    exit 1
fi
echo "[PASS] Function definition and return status"

# Test trap execution
OUT=$(./bin/aswell -c 'trap "echo TRAP_CALLED" EXIT; echo MAIN')
if ! echo "$OUT" | grep -q "TRAP_CALLED"; then
    echo "[FAIL] EXIT trap not executed: $OUT"
    exit 1
fi
echo "[PASS] EXIT signal trap"

# Test tilde and parameter expansions
OUT=$(./bin/aswell -c 'X="hello world"; echo "${X/world/aswell}"')
if [ "$OUT" != "hello aswell" ]; then
    echo "[FAIL] Parameter replacement mismatch: $OUT"
    exit 1
fi
echo "[PASS] Parameter pattern replacement"

echo ""
echo "5. Running Engine Animation & Demo Showcase Tests..."
./bin/aswell demo --auto
./bin/demo_engine --auto
echo "[PASS] Engine animation demo passed"

echo ""
echo "6. Running Colored Outputs & Commands Tests..."

# Test echo -e with octal (\033) and hex (\x1b) escape sequences
OUT=$(./bin/aswell -c 'echo -e "\033[31mRed\033[0m \x1b[32mGreen\x1b[0m"')
if ! echo "$OUT" | grep -q $'\033\[31mRed\033\[0m \033\[32mGreen\033\[0m'; then
    echo "[FAIL] echo -e colored escape sequences failed: $OUT"
    exit 1
fi
echo "[PASS] echo -e octal and hex ANSI color escape sequences"

# Test printf with ANSI color escapes and %b
OUT=$(./bin/aswell -c 'printf "\033[34m%s\033[0m %b\n" "Blue" "\033[33mYellow\033[0m"')
if ! echo "$OUT" | grep -q $'\033\[34mBlue\033\[0m \033\[33mYellow\033\[0m'; then
    echo "[FAIL] printf colored escape sequences failed: $OUT"
    exit 1
fi
echo "[PASS] printf octal, hex, and %b format specifier"

# Test color builtin with named colors, bold style, and hex
OUT=$(./bin/aswell -c 'color --bold red "Critical Alert"')
if ! echo "$OUT" | grep -q $'\033\[1m' || ! echo "$OUT" | grep -q "Critical Alert"; then
    echo "[FAIL] color builtin failed: $OUT"
    exit 1
fi
echo "[PASS] color builtin (bold, named colors, TrueColor)"

# Test color gradient output
OUT=$(./bin/aswell -c 'color gradient cyan magenta "Gradient Status"')
if ! echo "$OUT" | grep -q $'\033\[38;2;'; then
    echo "[FAIL] color gradient failed: $OUT"
    exit 1
fi
echo "[PASS] color gradient text interpolation"

# Test color rainbow output
OUT=$(./bin/aswell -c 'color rainbow "Rainbow Spectrum"')
if ! echo "$OUT" | grep -q $'\033\[38;2;'; then
    echo "[FAIL] color rainbow failed: $OUT"
    exit 1
fi
echo "[PASS] color rainbow output"

# Test color piped input from stdin
OUT=$(./bin/aswell -c 'printf "Line1\nLine2\n" | color green')
if ! echo "$OUT" | grep -q $'\033\[38;2;80;250;123mLine1' || ! echo "$OUT" | grep -q $'\033\[38;2;80;250;123mLine2'; then
    echo "[FAIL] color stdin pipeline failed: $OUT"
    exit 1
fi
echo "[PASS] color pipeline streaming from stdin"

# Test aswell color subcommand CLI interface
OUT=$(./bin/aswell color --bg "#111111" "#00f0ff" "Cyberpunk Direct")
if ! echo "$OUT" | grep -q "Cyberpunk Direct" || ! echo "$OUT" | grep -q $'\033\[38;2;0;240;255m'; then
    echo "[FAIL] aswell color CLI command failed: $OUT"
    exit 1
fi
echo "[PASS] aswell color CLI command"

# Test color list palette overview
./bin/aswell color list > /dev/null
echo "[PASS] color list palette display"

# Test brace expansion in command line execution
OUT=$(./bin/aswell -c 'echo {a,b}_{1,2}')
if [ "$OUT" != "a_1 a_2 b_1 b_2" ]; then
    echo "[FAIL] Brace Cartesian expansion mismatch: $OUT"
    exit 1
fi
OUT=$(./bin/aswell -c 'echo {01..05}')
if [ "$OUT" != "01 02 03 04 05" ]; then
    echo "[FAIL] Brace zero-padded range mismatch: $OUT"
    exit 1
fi
OUT=$(./bin/aswell -c 'echo a{b,c{1,2}}d')
if [ "$OUT" != "abd ac1d ac2d" ]; then
    echo "[FAIL] Nested brace expansion mismatch: $OUT"
    exit 1
fi
echo "[PASS] Brace expansion ({a,b}_{1,2}, {01..05}, a{b,c{1,2}}d)"

# Test directory stack (dirs, pushd, popd, options)
./bin/aswell -c 'dirs -c; pushd /tmp >/dev/null; dirs > /tmp/dirs_out.txt; popd >/dev/null; dirs >> /tmp/dirs_out.txt'
if ! grep -q "^/tmp " /tmp/dirs_out.txt; then
    echo "[FAIL] Directory stack pushd/popd failed"
    exit 1
fi
OUT=$(./bin/aswell -c 'dirs -c; pushd /tmp >/dev/null; pushd /var >/dev/null; dirs -v')
if ! echo "$OUT" | grep -q " 0  /var" || ! echo "$OUT" | grep -q " 1  /tmp"; then
    echo "[FAIL] dirs -v output mismatch: $OUT"
    exit 1
fi
echo "[PASS] Directory stack builtins (pushd, popd, dirs [-c|-v|-p])"

# Test command builtin bypassing functions and displaying info
OUT=$(./bin/aswell -c 'ls() { echo "func_override"; }; ls; command ls -d /tmp')
if ! echo "$OUT" | grep -q "func_override" || ! echo "$OUT" | grep -q "^/tmp"; then
    echo "[FAIL] command builtin function bypass failed: $OUT"
    exit 1
fi
OUT=$(./bin/aswell -c 'command -v cd; command -v ls')
if ! echo "$OUT" | grep -q "^cd" || ! echo "$OUT" | grep -q "ls"; then
    echo "[FAIL] command -v output mismatch: $OUT"
    exit 1
fi
OUT=$(./bin/aswell -c 'command -V cd')
if ! echo "$OUT" | grep -q "builtin"; then
    echo "[FAIL] command -V output mismatch: $OUT"
    exit 1
fi
OUT=$(./bin/aswell -c 'command -p -v ls')
if [ "$OUT" != "/bin/ls" ]; then
    echo "[FAIL] command -p -v ls mismatch: $OUT"
    exit 1
fi
echo "[PASS] command builtin (bypassing functions, -v, -V, -p)"

# Test history builtin
./bin/aswell -c 'history -c'
OUT=$(./bin/aswell -c 'history')
if [ -n "$OUT" ]; then
    echo "[FAIL] history -c did not clear history"
    exit 1
fi
echo "[PASS] history builtin (-c clear and management)"

echo ""
echo "7. Running Bash Compat, Smart CD, Hash, Umask & Getopts Tests..."

# Strict cd in scripts (POSIX): partial names must NOT resolve non-interactively
set +e
./bin/aswell -c 'cd /tmp/aswell_no_such_dir_xyz_123' 2>/dev/null
STATUS=$?
set -e
if [ "$STATUS" -eq 0 ]; then
    echo "[FAIL] non-interactive cd unexpectedly succeeded"
    exit 1
fi
echo "[PASS] Non-interactive cd stays strict (POSIX)"

# bash import smoke test with an isolated HOME (alias + export round-trip)
TMPHOME=$(mktemp -d)
printf 'alias smoket="echo SMOKE_ALIAS"\nexport SMOKE_VAR=smoke_value_42\n' > "$TMPHOME/.bashrc"
OUT=$(HOME="$TMPHOME" ./bin/aswell -c 'aswell bash import >/dev/null; alias smoket; echo "VAL=$SMOKE_VAR"')
rm -rf "$TMPHOME"
if ! echo "$OUT" | grep -q "alias smoket="; then
    echo "[FAIL] bash alias import failed: $OUT"
    exit 1
fi
if ! echo "$OUT" | grep -q "VAL=smoke_value_42"; then
    echo "[FAIL] bash export import failed: $OUT"
    exit 1
fi
echo "[PASS] aswell bash import (isolated HOME)"

# hash remembers and prints executable paths
OUT=$(./bin/aswell -c 'hash ls >/dev/null; hash -t ls')
if [ ! -x "$OUT" ]; then
    echo "[FAIL] hash -t ls did not print an executable: $OUT"
    exit 1
fi
set +e
OUT=$(./bin/aswell -c 'hash zz_no_such_cmd_xyz_123' 2>/dev/null)
STATUS=$?
set -e
if [ "$STATUS" -eq 0 ]; then
    echo "[FAIL] hash accepted an unknown command"
    exit 1
fi
echo "[PASS] hash builtin (remember, -t, reject unknown)"

# umask symbolic set and display round-trip
OUT=$(./bin/aswell -c 'umask u=rwx,go=; umask')
if [ "$OUT" != "0077" ]; then
    echo "[FAIL] symbolic umask set failed: $OUT"
    exit 1
fi
OUT=$(./bin/aswell -c 'umask 022 >/dev/null; umask -S')
if [ "$OUT" != "u=rwx,g=rx,o=rx" ]; then
    echo "[FAIL] umask -S display failed: $OUT"
    exit 1
fi
echo "[PASS] umask symbolic set and display"

# getopts end-to-end: classic while-loop over positional parameters
OUT=$(./bin/aswell -c 'while getopts "ab:" opt; do case $opt in a) echo GOT_A;; b) echo "GOT_B:$OPTARG";; ?) echo BAD;; :) echo MISSING;; esac; done; echo "IND=$OPTIND"' prog -a -bx -c)
echo "$OUT" | grep -q "GOT_A" || { echo "[FAIL] getopts flag -a: $OUT"; exit 1; }
echo "$OUT" | grep -q "GOT_B:x" || { echo "[FAIL] getopts attached arg: $OUT"; exit 1; }
echo "$OUT" | grep -q "BAD" || { echo "[FAIL] getopts invalid option: $OUT"; exit 1; }
echo "$OUT" | grep -q "IND=4" || { echo "[FAIL] getopts final OPTIND: $OUT"; exit 1; }
OUT=$(./bin/aswell -c 'while getopts ":ab:" opt; do case $opt in a) echo A;; b) echo "B:$OPTARG";; :) echo "MISS:$OPTARG";; ?) echo "BAD:$OPTARG";; esac; done' prog -a -z -b)
echo "$OUT" | grep -q "BAD:z" || { echo "[FAIL] getopts silent invalid: $OUT"; exit 1; }
echo "$OUT" | grep -q "MISS:b" || { echo "[FAIL] getopts silent missing arg: $OUT"; exit 1; }
echo "[PASS] getopts option parsing (flags, args, errors, OPTIND)"

# stty builtin: no-tty failure must be clean, help and errors sane
set +e
./bin/aswell -c 'stty' < /dev/null 2>/dev/null
STATUS=$?
set -e
if [ "$STATUS" -eq 0 ]; then
    echo "[FAIL] stty without a tty unexpectedly succeeded"
    exit 1
fi
OUT=$(./bin/aswell -c 'stty --help')
echo "$OUT" | grep -q "terminal line settings" || { echo "[FAIL] stty --help: $OUT"; exit 1; }
set +e
OUT=$(./bin/aswell -c 'stty bogusflag_xyz' < /dev/null 2>&1)
STATUS=$?
set -e
if [ "$STATUS" -eq 0 ]; then
    echo "[FAIL] stty bogus operand unexpectedly succeeded"
    exit 1
fi
echo "$OUT" | grep -q "invalid argument" || { echo "[FAIL] stty invalid operand: $OUT"; exit 1; }
echo "[PASS] stty builtin (no-tty failure, help, invalid operand)"

echo ""
echo "8. Running Customization Hub Tests (config / theme / doctor / reload)..."

CFGHOME=$(mktemp -d)
mkdir -p "$CFGHOME/.config/aswell"
run_cfg() { HOME="$CFGHOME" ./bin/aswell -c "$1" | strip_ansi; }
# Same as run_cfg but preserves the shell's exit status for negative tests.
run_cfg_status() {
    HOME="$CFGHOME" ./bin/aswell -c "$1" > /tmp/aswell_cfg_out.txt 2>&1
    local rc=$?
    strip_ansi < /tmp/aswell_cfg_out.txt
    return $rc
}
CFG_FILE="$CFGHOME/.config/aswell/config.txt"
fail_cfg() { echo "[FAIL] $1"; exit 1; }

# --- settings hub: set persists, get reads back, alias + toggle + validation
OUT=$(run_cfg 'aswell config set show_git false')
echo "$OUT" | grep -q "show_git = false" || fail_cfg "config set output: $OUT"
grep -q '^show_git=false$' "$CFG_FILE" || fail_cfg "config set did not persist"
[ "$(run_cfg 'aswell config get show_git')" = "false" ] || fail_cfg "config get mismatch"

# CLI form (before the shell starts) behaves identically
[ "$(HOME="$CFGHOME" ./bin/aswell config get show_git)" = "false" ] || fail_cfg "CLI config get mismatch"

# toggling flips a boolean
run_cfg 'aswell config toggle show_git' >/dev/null
[ "$(run_cfg 'aswell config get show_git')" = "true" ] || fail_cfg "config toggle failed"

# aliases are accepted and stored under their canonical name
run_cfg 'aswell config set vim true' >/dev/null
grep -q '^vi_mode=true$' "$CFG_FILE" || fail_cfg "alias 'vim' did not canonicalise to vi_mode"

# invalid values and unknown keys are rejected loudly
set +e
OUT=$(run_cfg_status 'aswell config set animation maybe'); STATUS=$?
set -e
[ "$STATUS" -ne 0 ] || fail_cfg "bad boolean accepted"
echo "$OUT" | grep -q "not a boolean" || fail_cfg "missing boolean error: $OUT"
set +e
OUT=$(run_cfg_status 'aswell config set animtion true'); STATUS=$?
set -e
[ "$STATUS" -ne 0 ] || fail_cfg "unknown key accepted"
echo "$OUT" | grep -q "did you mean 'animation'" || fail_cfg "missing did-you-mean: $OUT"
[ "$(run_cfg 'aswell config get show_git')" = "true" ] || fail_cfg "rejected write changed the config"

# hand-written config.txt keeps its comments when a value changes
printf '# my notes\ntheme=nord\n' > "$CFG_FILE"
run_cfg 'aswell config set show_jobs false' >/dev/null
grep -q '# my notes' "$CFG_FILE" || fail_cfg "comments were destroyed by config set"
grep -q '^theme=nord$' "$CFG_FILE" || fail_cfg "unrelated key was lost"

# help is generated from the registry, so every setting is documented
run_cfg 'aswell config help' | grep -q 'autosuggestions' || fail_cfg "config help missing a setting"
run_cfg 'aswell config help show_git' | grep -q 'Show the <git> branch badge' || fail_cfg "config help <key> mismatch"

# JSON view for scripting
OUT=$(run_cfg 'aswell config list --json')
echo "$OUT" | head -1 | grep -q '^{' || fail_cfg "config json does not start with { "
echo "$OUT" | tail -1 | grep -q '^}' || fail_cfg "config json does not end with }"
echo "$OUT" | grep -q '"show_jobs": {"value": false' || fail_cfg "config json missing changed value: $OUT"

# export / import round-trip
run_cfg 'aswell config export --to /tmp/aswell_exported_config.txt' >/dev/null
grep -q '# Aswell shell configuration' /tmp/aswell_exported_config.txt || fail_cfg "export header missing"
IMPORTHOME=$(mktemp -d)
OUT=$(HOME="$IMPORTHOME" ./bin/aswell -c 'aswell config import /tmp/aswell_exported_config.txt' | strip_ansi)
echo "$OUT" | grep -q 'imported' || fail_cfg "import output: $OUT"
[ "$(HOME="$IMPORTHOME" ./bin/aswell -c 'aswell config get show_jobs' | strip_ansi)" = "false" ] || fail_cfg "imported value mismatch"
rm -rf "$IMPORTHOME"

# --- themes: switching persists (regression: in-shell `theme set` used to be a no-op)
OUT=$(run_cfg 'aswell theme set matrix')
echo "$OUT" | grep -q "theme = matrix" || fail_cfg "theme set output: $OUT"
grep -q '^theme=matrix$' "$CFG_FILE" || fail_cfg "theme set did not persist"
[ "$(run_cfg 'aswell config get theme')" = "matrix" ] || fail_cfg "theme not readable via config get"

run_cfg 'aswell theme set nord' >/dev/null
grep -q '^theme=nord$' "$CFG_FILE" || fail_cfg "bare 'aswell theme <name>' shortcut failed"
run_cfg 'aswell theme reset' >/dev/null
grep -q '^theme=modern$' "$CFG_FILE" || fail_cfg "theme reset failed"

# listing discovers built-ins *and* user stylesheets
run_cfg 'aswell theme list' | grep -q 'matrix' || fail_cfg "theme list missing built-in"
run_cfg 'aswell theme new mydesk --from nord' >/dev/null
test -f "$CFGHOME/.config/aswell/themes/mydesk.css" || fail_cfg "theme new did not scaffold a stylesheet"
run_cfg 'aswell theme list' | grep -q 'mydesk' || fail_cfg "theme list missing user theme"
run_cfg 'aswell theme set mydesk' >/dev/null
grep -q '^theme=mydesk$' "$CFG_FILE" || fail_cfg "custom theme not selectable"
run_cfg 'aswell config set theme nord' >/dev/null

# unknown theme names are rejected with a suggestion
set +e
OUT=$(run_cfg_status 'aswell theme set dracul'); STATUS=$?
set -e
[ "$STATUS" -ne 0 ] || fail_cfg "unknown theme accepted"
echo "$OUT" | grep -q "did you mean 'aswell theme set dracula'" || fail_cfg "theme suggestion missing: $OUT"

# previews render through the real prompt engine (TrueColor ANSI on stdout)
COLORTERM=truecolor ./bin/aswell --config "$CFGHOME/.config/aswell" theme preview cyberpunk > /tmp/aswell_preview.txt 2>&1
grep -q $'\033\[38;2;' /tmp/aswell_preview.txt || fail_cfg "theme preview produced no TrueColor styling"
grep -q 'cyberpunk' /tmp/aswell_preview.txt || fail_cfg "theme preview missing theme name"
rm -f /tmp/aswell_preview.txt
run_cfg 'aswell theme preview --all' | grep -q 'powerline' || fail_cfg "theme preview --all incomplete"
echo "$CFGHOME" > /tmp/aswell_cfghome_marker

# --- doctor: reports problems, stays silent when clean
CLEANHOME=$(mktemp -d)
mkdir -p "$CLEANHOME/.config/aswell"
printf 'theme=nord\nanimation=true\n' > "$CLEANHOME/.config/aswell/config.txt"
set +e
HOME="$CLEANHOME" ./bin/aswell -c 'aswell doctor' > /tmp/doctor_clean.txt 2>&1
STATUS=$?
set -e
[ "$STATUS" -eq 0 ] || fail_cfg "doctor reported problems on a clean config: $(cat /tmp/doctor_clean.txt)"
grep -q "Everything checks out" /tmp/doctor_clean.txt || fail_cfg "doctor clean summary missing"

printf 'animaton=true\nshow_git=maybe\n' > "$CLEANHOME/.config/aswell/config.txt"
set +e
HOME="$CLEANHOME" ./bin/aswell -c 'aswell doctor --quiet' > /tmp/doctor_bad.txt 2>&1
STATUS=$?
set -e
[ "$STATUS" -ne 0 ] || fail_cfg "doctor accepted a broken config"
grep -q "animaton" /tmp/doctor_bad.txt || fail_cfg "doctor missed unknown key: $(cat /tmp/doctor_bad.txt)"
grep -q "aswell config set show_git" /tmp/doctor_bad.txt || fail_cfg "doctor missing pasteable fix: $(cat /tmp/doctor_bad.txt)"

# broken prompt templates are caught before they blank the prompt
printf '<prompt class="main">\n  <usr />\n' > "$CLEANHOME/.config/aswell/prompt.html"
set +e
HOME="$CLEANHOME" ./bin/aswell -c 'aswell doctor' > /tmp/doctor_html.txt 2>&1
STATUS=$?
set -e
[ "$STATUS" -ne 0 ] || fail_cfg "doctor accepted an unclosed prompt.html"
grep -q "never closed" /tmp/doctor_html.txt || fail_cfg "doctor missing unclosed-tag hint: $(cat /tmp/doctor_html.txt)"
grep -q "unknown element <usr>" /tmp/doctor_html.txt || fail_cfg "doctor missing unknown tag hint: $(cat /tmp/doctor_html.txt)"

# bad CSS properties are named with the file and line
printf 'user { colr: #ff0000; }\ndirectory { color: #00ff00; }\n' > "$CLEANHOME/.config/aswell/theme.css"
HOME="$CLEANHOME" ./bin/aswell -c 'aswell doctor' > /tmp/doctor_css.txt 2>&1 || true
grep -q "colr" /tmp/doctor_css.txt || fail_cfg "doctor missed unknown CSS property: $(cat /tmp/doctor_css.txt)"
rm -rf "$CLEANHOME"

# --- reload + config dir plumbing
OUT=$(run_cfg 'aswell reload')
echo "$OUT" | grep -q "no live prompt" || fail_cfg "reload outside a session should say so: $OUT"
[ "$(./bin/aswell --config /tmp/aswell_alt_config config path | strip_ansi)" = "/tmp/aswell_alt_config" ] || fail_cfg "--config PATH not honoured"
mkdir -p /tmp/aswell_alt_config && HOME="$CFGHOME" ./bin/aswell --config /tmp/aswell_alt_config -c 'aswell config set theme nord' >/dev/null
grep -q '^theme=nord$' /tmp/aswell_alt_config/config.txt || fail_cfg "--config PATH not used for writes"
rm -rf /tmp/aswell_alt_config

# --- env override for one-off theme experiments
# ASWELL_THEME overrides the rendered theme for one session without touching the file
run_cfg 'aswell config set theme nord' >/dev/null
ASWELL_THEME=minimal HOME="$CFGHOME" ./bin/aswell theme list | strip_ansi | grep -q $'\xe2\x9c\x93 minimal' || fail_cfg "ASWELL_THEME override not reflected in theme list"
[ "$(run_cfg 'aswell config get theme')" = "nord" ] || fail_cfg "ASWELL_THEME leaked into the stored setting"

rm -f /tmp/aswell_cfghome_marker /tmp/aswell_exported_config.txt
rm -rf "$CFGHOME"
echo "[PASS] customization hub (config, theme, doctor, reload)"

echo ""
echo "=================================================="
echo "       ALL ASWELL TESTS PASSED SUCCESSFULLY!      "
echo "=================================================="
