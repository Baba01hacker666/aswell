#!/bin/bash
set -e

echo "=================================================="
echo "          ASWELL TEST SUITE EXECUTION             "
echo "=================================================="

BUILD_DIR="bin"
mkdir -p "$BUILD_DIR"

echo "1. Building Unit Tests..."
g++ -std=c++20 -O2 -Iinclude tests/test_lexer.cpp src/shell/lexer.o src/shell/parser.o src/shell/executor.o src/shell/expansion.o src/shell/environment.o src/shell/jobs.o src/shell/signals.o src/shell/builtins.o -o bin/test_lexer
g++ -std=c++20 -O2 -Iinclude tests/test_parser.cpp src/shell/lexer.o src/shell/parser.o src/shell/executor.o src/shell/expansion.o src/shell/environment.o src/shell/jobs.o src/shell/signals.o src/shell/builtins.o -o bin/test_parser
g++ -std=c++20 -O2 -Iinclude tests/test_expansion.cpp src/shell/expansion.o src/shell/environment.o src/shell/signals.o -o bin/test_expansion
g++ -std=c++20 -O2 -Iinclude tests/test_css.cpp src/ui/css_parser.o src/ui/color.o -o bin/test_css
g++ -std=c++20 -O2 -Iinclude tests/test_ui.cpp src/ui/dom.o src/ui/layout.o src/ui/render.o src/ui/color.o src/ui/animation.o src/ui/css_parser.o src/ui/prompt.o src/ui/terminal.o src/shell/environment.o -o bin/test_ui

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
echo "=================================================="
echo "       ALL ASWELL TESTS PASSED SUCCESSFULLY!      "
echo "=================================================="
