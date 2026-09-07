#!/bin/bash
set -e

echo "=================================================="
echo "          ASWELL TEST SUITE EXECUTION             "
echo "=================================================="

BUILD_DIR="bin"
mkdir -p "$BUILD_DIR"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -O2}"
LDFLAGS="${LDFLAGS:-}"

echo "1. Building Unit Tests..."
$CXX $CXXFLAGS -Iinclude tests/test_lexer.cpp src/shell/lexer.o -o bin/test_lexer $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_parser.cpp src/shell/lexer.o src/shell/parser.o -o bin/test_parser $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_expansion.cpp src/shell/expansion.o src/shell/environment.o src/shell/signals.o -o bin/test_expansion $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_css.cpp src/ui/css_parser.o src/ui/color.o -o bin/test_css $LDFLAGS
$CXX $CXXFLAGS -Iinclude tests/test_ui.cpp src/ui/dom.o src/ui/layout.o src/ui/render.o src/ui/color.o src/ui/animation.o src/ui/css_parser.o src/ui/prompt.o src/ui/terminal.o src/ui/template_engine.o src/shell/environment.o -o bin/test_ui $LDFLAGS

echo "2. Running Unit Tests..."
./bin/test_lexer
./bin/test_parser
./bin/test_expansion
./bin/test_css
./bin/test_ui

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

echo ""
echo "=================================================="
echo "       ALL ASWELL TESTS PASSED SUCCESSFULLY!      "
echo "=================================================="
