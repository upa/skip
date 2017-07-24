

ip=../iproute2-4.10.0/ip/ip
libskip=`cd ../tools && pwd`/libskip.so
nsname=skip-test

# setup test namespace
if [ ! -e /var/run/netns/$nsname ]; then
	$ip netns add $nsname
fi
$ip netns exec $nsname ifconfig lo up
$ip netns exec $nsname \
	$ip route add to 0.0.0.0/0 dev lo \
	encap skip host 127.0.0.1 inbound outbound


echo Executing nc port 10000 with AF_SKIP from netns $nsname
LD_PRELOAD=$libskip \
	AF_SKIP_BIND_ADDRESS=10.0.0.1 \
	nc -l 10000 &
nc_pid=$!
sleep 1
echo


echo LISTEN sockets in netns
$ip netns exec $nsname \
	netstat -an | grep LISTEN | grep tcp
echo


echo LISTEN socket in host name space
netstat -an | grep LISTEN | grep tcp


kill -KILL $nc_pid
