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

TEST 'Identify processes'
for REPLY in $(
  exec sh -c 'echo $$ ; exec ./k9 -T -i -- sh -c '\''echo $$'\' |
  {
    read REALPARENT
    read PARENT ; read CHILD
    read REALCHILD
    echo "$REALPARENT/$PARENT $REALCHILD/$CHILD"
  }) ; do

  [ x"${REPLY%% *}" = x"${REPLY#* }" ]
done

TEST 'Exit code propagation'
TEST_EXIT 2 ./k9 -T -- sh -c 'exit 2'

TEST 'Signal propagation'
TEST_EXIT $((128 + 9)) ./k9 -T -- sh -c 'echo $$ ; kill -9 $$'

TEST 'Untethered child process'
[ $(./k9 -T -u -- ls -l /proc/self/fd | grep '[0-9]-[0-9]' | wc -l) -eq 4 ]

TEST 'Untethered child process with 8M data'
[ $(./k9 -T -u -- dd if=/dev/zero bs=8K count=1000 | wc -c) -eq 8192000 ]

TEST 'Tether with new file descriptor'
[ $(./k9 -T -f - -- ls -l /proc/self/fd | grep '[0-9]-[0-9]' | wc -l) -eq 5 ]

TEST 'Tether using stdout'
[ $(./k9 -T -- ls -l /proc/self/fd | grep '[0-9]-[0-9]' | wc -l) -eq 4 ]

TEST 'Tether using named stdout'
[ $(./k9 -T -f 1 -- ls -l /proc/self/fd | grep '[0-9]-[0-9]' | wc -l) -eq 4 ]

TEST 'Tether using stdout with 8M data'
[ $(./k9 -T -- dd if=/dev/zero bs=8K count=1000 | wc -c) -eq 8192000 ]

TEST 'Tether quietly using stdout with 8M data'
[ $(./k9 -T -q -- dd if=/dev/zero bs=8K count=1000 | wc -c) -eq 0 ]

TEST 'Tether named in environment'
[ $(./k9 -T -n TETHER -- printenv | grep TETHER) = "TETHER=1" ]

TEST 'Tether named alone in argument'
[ $(./k9 -T -n @tether@ -- echo @tether@ | grep '1') = "1" ]

TEST 'Tether named as suffix in argument'
[ $(./k9 -T -n @tether@ -- echo x@tether@ | grep '1' ) = "x1" ]

TEST 'Tether named as prefix argument'
[ $(./k9 -T -n @tether@ -- echo @tether@x | grep '1' ) = "1x" ]

TEST 'Tether named as infix argument'
[ $(./k9 -T -n @tether@ -- echo x@tether@x | grep '1' ) = "x1x" ]

TEST 'Unexpected death of child'
REPLY=$(
  exec 3>&1
  {
    if ./k9 -T -d -i -- sh -c 'while : ; do : ; done ; exit 0' 3>&- ; then
      echo 0 >&3
    else
      echo $? >&3
    fi
  } | {
    read PID
    kill -- "$PID"
  })
[ x"$REPLY" = x$((128 + 15)) ]

TEST 'Fast signal queueing'
REPLY=$(
  SIGNALS="1 2 3 15"
  sh -c "
    echo \$\$
    exec ./k9 -T -d -- sh -c '
      C=0
      trap '\\'': \$(( ++C ))'\\'' $SIGNALS
      echo \$\$
      while [ \$C -ne $(echo $SIGNALS | wc -w) ] ; do sleep 1 ; done
      echo \$C'" |
  {
    read PARENT
    read CHILD
    for SIG in $SIGNALS ; do
        kill -$SIG -- "$PARENT"
    done
    read COUNT
    echo "$COUNT"
  })
[ x"$REPLY" = x"4" ]

TEST 'Slow signal queueing'
REPLY=$(
  SIGNALS="1 2 3 15"
  sh -c "
    echo \$\$
    exec ./k9 -T -d -- sh -c '
      C=0
      trap '\\'': \$(( ++C ))'\\'' $SIGNALS
      echo \$\$
      while [ \$C -ne $(echo $SIGNALS | wc -w) ] ; do sleep 1 ; done
      echo \$C'" |
  {
    read PARENT
    read CHILD
    for SIG in $SIGNALS ; do
        kill -$SIG -- "$PARENT"
        sleep 1
    done
    read COUNT
    echo "$COUNT"
  })
[ x"$REPLY" = x"4" ]

TEST 'Timeout with data that must be flushed after 6s'
REPLY=$(
    /usr/bin/time -p ./k9 -T -t 4 -- sh -c 'trap "" 15 ; sleep 6' 2>&1 |
    grep real)
REPLY=${REPLY%%.*}
REPLY=${REPLY##* }
[ "$REPLY" -ge 6 ]

./k9 -T -p pidfile -- true
