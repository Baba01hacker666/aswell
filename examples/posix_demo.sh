#!/usr/bin/env aswell
# shellcheck shell=sh
# POSIX Shell Compatibility Demonstration

echo "Starting POSIX compatibility demo in Aswell..."

# 1. Variables and Parameter Expansion
NAME="Aswell"
VERSION="1.0.0"
echo "Shell: $NAME v$VERSION"
echo "Length of NAME: ${#NAME}"
echo "Default test: ${UNSET_VAR:-default_value}"
FILE="/path/to/script.sh"
echo "Basename using % strip: ${FILE##*/}"
echo "Dirname using % strip: ${FILE%/*}"

# 2. Arithmetic Expansion
A=15
B=4
echo "Arithmetic $(( (A * 2 + B) / 3 ))"
# shellcheck disable=SC3019
echo "Power: $(( 2 ** 8 ))"
echo "Ternary: $(( A > 10 ? 100 : 0 ))"

# 3. Pipelines & Redirections
printf "Line 3\nLine 1\nLine 2\n" | sort | while read -r line; do
    echo "Sorted: $line"
done

# 4. Here-documents
cat << 'EOF_HEREDOC'
This is a here-document
running completely natively
inside Aswell Shell!
EOF_HEREDOC

# 5. Functions & Positional parameters
greet() {
    echo "Hello, $1! Welcome to $NAME."
}
greet "Developer"

echo "Demo completed successfully!"
