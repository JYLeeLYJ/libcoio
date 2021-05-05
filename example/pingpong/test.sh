#!/bin/sh

killall pingpong_server
for connections in 1000 ; do
    sleep 1
    taskset -c 1 ./pingpong_server 1 > ./server_log.txt 2>&1 & srvpid=$!
    echo ------- connections per thread: $connections ---------
    sleep 1
    ./pingpong_client 4 600 $connections
    echo =-----------------------------------------------------
    kill -9 $srvpid
done
