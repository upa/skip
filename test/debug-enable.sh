#!/bin/sh

# enable pr_debug()
af_skip=`cd ../kmod && pwd`/af_skip.c
skip_lwt=`cd ../kmod && pwd`/skip_lwt.c
debugctl="/sys/kernel/debug/dynamic_debug/control"
echo -n "file $af_skip  +p"  > $debugctl
echo -n "file $skip_lwt  +p" > $debugctl
