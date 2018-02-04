#!/usr/bin/env bash

set -eu

[ -z "${0##/*}" ] || exec "$PWD/$0" "$@"

PS4='+ [$BASHPID:\t] '

say()
{
    printf '%s\n' "$1"
}

die()
{
    say "${0##*/}: $1" >&2
    exit 1
}

usage()
{
    say "usage: $0 cmd ..." >&2
    exit 1
}

valgrind()
{
    [ -x "$1" ] || die "Unable to execute $1"

    # Execute program under valgrind taking RPATH into account
    #
    # Normalise the content of the environment, and handle the RPATH to
    # find the location of the program interpreter if specified.
    #
    # If RPATH is configured, and the program interpreter is specified
    # using a relative path, use it to find the appropriate program
    # interpreter.
    #
    # Usually RPATH is set by the --with-test-environment configuration
    # option.

    if [ -n "${RPATH++}" ] ; then
        set -- "$(
            readelf -l "$1" |
            grep -F 'Requesting program interpreter')" "$@"
        set -- "${1%]*}"  "${@:2}"
        set -- "${1#*: }" "${@:2}"
        if [ -z "${1##/*}" ] ; then
            shift
        else
            set -- "$(
                IFS=:
                for LDSO in $RPATH ; do
                    LDSO="$LDSO/$1"
                    ! [ -x "$LDSO" ] || { echo "$LDSO" ; exit 0 ; }
                done
            )" "${@:2}"
            [ -n "$1" ] || exit 1
            set -- "$1" --library-path "$RPATH" --inhibit-rpath "$2" "${@:2}"
        fi
    fi

    # Disable ASLR so that valgrind will avoid mmap() collisions with vsdo
    set -- setarch "$(uname -m)" -R "$@"
    set -- valgrind                 "$@"

    # Find the execution prefix, and rotate it to the front of the command
    while [ "$#" -ne 0 ] ; do
        [ x-- != x"${@:$#:1}" ] || {
            set -- "${@:1:$(($#-1))}"
            break
        }
        set -- "${@:$#:1}" "${@:1:$(($#-1))}"
    done

    set -- libtool --mode=execute        "$@"
    set -- PATH="$PATH"                  "$@"
    set -- ${USER+      USER="$USER"}    "$@"
    set -- ${LOGNAME+LOGNAME="$LOGNAME"} "$@"
    set -- ${HOME+      HOME="$HOME"}    "$@"
    set -- ${LANG+      LANG="$LANG"}    "$@"
    set -- ${SHELL+    SHELL="$SHELL"}   "$@"
    set -- ${TERM+      TERM="$TERM"}    "$@"

    exec env - "$@"
}

[ $# -ne 0 ] || usage

terminate()
{
    set -x

    ps wwwaxjfh

    if [ -n "$!" ] ; then
        ps -o ppid "$!"
        set -- "$1" $(ps -o ppid "$!" | tail -n 1)
        [ x"$$" != x"$2" ] || {
            kill -SIGTERM -- -"$!"
            sleep 2
        }
        set -- "$1" $(ps -o ppid "$!" | tail -n 1)
        [ x"$$" != x"$2" ] || {
            kill -SIGKILL -- -"$!"
        }
    fi

    ps wwwaxjfh

    trap - "$1"
    kill -SIGKILL -- -$$
    exit 1
}

trap "terminate SIGTERM" SIGTERM
trap "terminate SIGINT"  SIGINT
trap "terminate SIGQUIT" SIGQUIT

exec > >(tee "$1.log") 2>&1 # Note: Set $! here, but will be redefined later

rm -rf -- "$1.strace"
mkdir -- "$1.strace"

set -m
(
    set -x
    valgrind "$@" -- # strace -ff -t -o "$1.strace/log"
) &

RC=0
wait $! || RC=$?

if [ 0 -eq "$RC" ] ; then
    rm -rf -- "$1.strace"
    say "PASS"
else
    say "FAIL"
fi
exit "$RC"
