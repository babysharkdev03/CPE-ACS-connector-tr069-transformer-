#!/bin/sh
set -eu

db="${UCI_TEST_DB:?UCI_TEST_DB is required}"
staged="${db}.staged"
[ "${1:-}" = "-q" ] && shift
command="${1:-}"
[ "$#" -gt 0 ] && shift

active_db() {
    [ -f "$staged" ] && printf '%s' "$staged" || printf '%s' "$db"
}

lookup() {
    key="$1"
    file="$(active_db)"
    [ -f "$file" ] || return 1
    while IFS='=' read -r current value; do
        if [ "$current" = "$key" ]; then
            printf '%s\n' "$value"
            return 0
        fi
    done <"$file"
    return 1
}

ensure_staged() {
    [ -f "$staged" ] || cp "$db" "$staged"
}

replace_value() {
    key="$1"
    value="$2"
    ensure_staged
    temporary="${staged}.$$.tmp"
    found=0
    : >"$temporary"
    while IFS='=' read -r current old_value; do
        [ -n "$current" ] || continue
        if [ "$current" = "$key" ]; then
            printf '%s=%s\n' "$key" "$value" >>"$temporary"
            found=1
        else
            printf '%s=%s\n' "$current" "$old_value" >>"$temporary"
        fi
    done <"$staged"
    [ "$found" -eq 1 ] || printf '%s=%s\n' "$key" "$value" >>"$temporary"
    mv "$temporary" "$staged"
}

delete_value() {
    key="$1"
    ensure_staged
    temporary="${staged}.$$.tmp"
    : >"$temporary"
    while IFS='=' read -r current old_value; do
        [ "$current" = "$key" ] || printf '%s=%s\n' "$current" "$old_value" >>"$temporary"
    done <"$staged"
    mv "$temporary" "$staged"
}

case "$command" in
    get)
        lookup "$1"
        ;;
    set)
        assignment="$1"
        replace_value "${assignment%%=*}" "${assignment#*=}"
        ;;
    delete)
        delete_value "$1"
        ;;
    commit)
        [ -f "$staged" ] && mv "$staged" "$db"
        ;;
    revert)
        rm -f "$staged"
        ;;
    *)
        exit 1
        ;;
esac
