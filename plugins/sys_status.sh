#!/bin/sh
# shellcheck shell=sh
# Aswell Plugin: Live System Metrics & Lifecycle Hooks
# Demonstrates dynamic state computation ("the JS logic") driving HTML & CSS design power

aswell_on_prompt() {
    # 1. Live system load
    if [ -r /proc/loadavg ]; then
        read -r l1 _ < /proc/loadavg
        export SYS_LOAD="$l1"
    fi

    # 2. Live memory usage %
    if [ -r /proc/meminfo ]; then
        total=$(awk '/MemTotal/ {print $2}' /proc/meminfo 2>/dev/null)
        avail=$(awk '/MemAvailable/ {print $2}' /proc/meminfo 2>/dev/null)
        if [ -n "$total" ] && [ -n "$avail" ] && [ "$total" -gt 0 ]; then
            used=$(( (total - avail) * 100 / total ))
            export SYS_MEM="${used}%"
        fi
    fi
}

aswell_after_command() {
    cmd="$1"
    dur="$2"
    status="$3"
    if [ "$status" -ne 0 ]; then
        export LAST_FAILED_CMD="$cmd"
    fi
}
