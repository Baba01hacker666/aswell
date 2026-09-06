#!/usr/bin/env aswell
# shellcheck shell=sh
# Loops, Conditionals, and Functions in Aswell

count=1
while [ "$count" -le 3 ]; do
    echo "While iteration: $count"
    count=$((count + 1))
done

for fruit in apple banana cherry; do
    echo "Fruit: $fruit"
done

number=42
if [ "$number" -eq 42 ]; then
    echo "The answer to life, the universe, and everything!"
elif [ "$number" -lt 42 ]; then
    echo "Too low"
else
    echo "Too high"
fi

case "$fruit" in
    apple) echo "Matched apple" ;;
    cherry) echo "Matched cherry!" ;;
    *) echo "Unknown fruit" ;;
esac
