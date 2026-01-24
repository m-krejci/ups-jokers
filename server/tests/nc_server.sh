#!/bin/bash

if [ "$#" -ne 2 ]; then 
    echo "Použití: $0 <ip_adresa> <port>"
    exit 1
fi

IP=$1
PORT=$2

echo "Připojuji se k $IP:$PORT..."


{
    echo -n "JOKELOGI0004user"
    sleep 2

    echo -n "JOKERCRT0004room"
    sleep 2

    cat
} | nc $IP $PORT