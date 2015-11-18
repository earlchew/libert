#!/bin/sh

set -e

TEST()
{
    printf '\nTEST - %s\n' "$1"
}

TEST_EXIT()
{
    if ( shift ; "$@" )
    then false ; else [ $? -eq $1 ] ; fi
}

TEST 'Usage'
TEST_EXIT 1 ./k9 -? -- true

TEST 'Missing -P option value'
TEST_EXIT 1 ./k9 -P

set -x
TEST 'Illegal negative -P option value'
TEST_EXIT 1 ./k9 -P -2 -- true

TEST 'Valid -P option values'
./k9 -P -1 -- true
./k9 -P 0 -- true
./k9 -P 1 -- true

TEST 'Long --pid option'
./k9 --pid 1 -- true

TEST 'Missing -p option value'
TEST_EXIT 1 ./k9 -p

TEST 'Valid -p option value'
./k9 -d -p pidfile -- true

TEST 'Long --pidfile option'
./k9 --pidfile pidfile -- true

TEST 'Missing command'
TEST_EXIT 1 ./k9

TEST 'Simple command'
REPLY=$(./k9 echo test)
[ x"$REPLY" = x"test" ]

TEST 'Simple command in test mode'
REPLY=$(./k9 -T echo test)
[ x"$REPLY" = x"test" ]

TEST 'Empty pid file'
rm -f pidfile
: > pidfile
./k9 -T -p pidfile -- true

TEST 'Invalid content in pid file'
rm -f pidfile
dd if=/dev/zero bs=1K count=1 > pidfile
./k9 -T -p pidfile -- true

TEST 'Dead process in pid file'
rm -f pidfile
sh -c 'echo $$' > pidfile
./k9 -T -d -p pidfile -- true

TEST 'Aliased process in pid file'
rm -f pidfile
( sh -c 'echo $(( $$ + 1))' > pidfile ; sleep 1 ) &
sleep 1
./k9 -T -d -p pidfile -- true
wait

TEST 'Read non-existent pid file'
rm -f pidfile
TEST_EXIT 1 ./k9 -T -p pidfile

TEST 'Read malformed pid file'
rm -f pidfile
date > pidfile
TEST_EXIT 1 ./k9 -T -p pidfile

TEST 'Exit code propagation'
TEST_EXIT 2 ./k9 -T -- sh -c 'exit 2'

TEST 'Signal propagation'
TEST_EXIT $((128 + 9)) ./k9 -T -- sh -c 'echo $$ ; kill -9 $$'

TEST 'File descriptor propagation'
[ $(./k9 -T -- ls -l /proc/self/fd | grep '[0-9]-[0-9]' | wc -l) -eq 5 ]

TEST 'Untethered child process'
[ $(./k9 -T -f - -- ls -l /proc/self/fd | grep '[0-9]-[0-9]' | wc -l) -eq 4 ]

TEST 'Untethered child process with 8M data'
[ $(./k9 -T -f - -- dd if=/dev/zero bs=8K count=1000 | wc -c) -eq 8192000 ]

TEST 'Tether using stdout'
[ $(./k9 -T -s -- ls -l /proc/self/fd | grep '[0-9]-[0-9]' | wc -l) -eq 4 ]

TEST 'Tether using stdout with 8M data'
[ $(./k9 -T -s -- dd if=/dev/zero bs=8K count=1000 | wc -c) -eq 8192000 ]

TEST 'Timeout with data that must be flushed after 6s'
REPLY=$(
    /usr/bin/time -p ./k9 -T -t 4 -- sh -c 'trap "" 15 ; sleep 6' 2>&1 |
    grep real)
REPLY=${REPLY%%.*}
REPLY=${REPLY##* }
[ "$REPLY" -ge 6 ]

./k9 -T -p pidfile -- true
