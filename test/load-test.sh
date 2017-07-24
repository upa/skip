#!/bin/bash

ip=../iproute2-4.10.0/ip/ip
libskip=`cd ../tools && pwd`/libskip.so
kmod=../kmod/skip.ko


function debug_enable () {
	af_skip=`cd ../kmod && pwd`/af_skip.c
	skip_lwt=`cd ../kmod && pwd`/skip_lwt.c
	debugctl="/sys/kernel/debug/dynamic_debug/control"
	echo -n "file $af_skip  +p"  > $debugctl
	echo -n "file $skip_lwt  +p" > $debugctl
}


echo test: load and unload
insmod $kmod
debug_enable
sleep 0.5
rmmod $kmod
ret=$?

if [ ! $ret -eq 0 ]; then
	echo exit status $ret
	exit
fi
echo


echo test: load, add route, del route and unload
insmod $kmod
debug_enable
$ip route add 172.16.0.0/16 dev lo encap skip host 127.0.0.1
sleep 0.5
$ip route del 172.16.0.0/16
sleep 0.5
rmmod $kmod
ret=$?

if [ ! $ret -eq 0 ]; then
	echo exit status $ret
	exit
fi
echo


echo test: load, add route, bind socket, close socket, del route and unload
insmod $kmod
debug_enable
$ip route add 172.16.0.0/16 dev lo encap skip host 127.0.0.1
sleep 0.5
LD_PRELOAD=$libskip \
        AF_SKIP_BIND_ADDRESS=172.16.0.1 \
        nc -l 10000 &
nc_pid=$!
sleep 0.5
kill -KILL $nc_pid
$ip route del 172.16.0.0/16
sleep 0.5
rmmod $kmod
ret=$?

if [ ! $ret -eq 0 ]; then
	echo exit status $ret
	exit
fi
echo
