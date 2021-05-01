#!/bin/sh

killall pingpong_server
for sessions in 1 ; do
    sleep 2
    echo "session count : $sessions"
    taskset -c 1 ./pingpong_server 1 >2 /dev/null & srvpid=$!
    sleep 1
    taskset -c 2 ./pingpong_client 1 1 $sessions
    kill -9 $srvpid
done
