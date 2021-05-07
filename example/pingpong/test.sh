#!/bin/sh

killall pingpong_server
for connections in 1000 ; do
    sleep 1
    ./pingpong_server > ./server_log.txt 2>&1 & srvpid=$!
    echo ------- connections per thread: $connections ---------
    sleep 1
    #remember disable taskset when enable sq_polling  
    taskset -c 2,3 ./pingpong_client 4 500 $connections 8888
    echo =-----------------------------------------------------
    kill -9 $srvpid

done
