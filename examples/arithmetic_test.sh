#!/usr/bin/env aswell
# shellcheck shell=sh
# Advanced Arithmetic Expressions in Aswell

X=10
Y=20
Z=$(( (X + Y) * 2 - (Y / 2) ))
echo "Z = $Z"

BIT=$(( (1 << 4) | (1 << 2) ))
echo "Bitwise: $BIT"

COMP=$(( (X < Y) && (Y == 20) ))
echo "Comparison result: $COMP"
