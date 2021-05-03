#!/bin/sh

killall pingpong_server
for sessions in 1 ; do
    sleep 1
    echo "session count : $sessions"
    taskset -c 1 ./pingpong_server 1 > ./server_log.txt 2>&1 & srvpid=$!
    echo ------------- session count = $sessions
    sleep 1
    taskset -c 2 ./pingpong_client 1 1000 $sessions
    echo =---------------------------------
    kill -9 $srvpid
done
