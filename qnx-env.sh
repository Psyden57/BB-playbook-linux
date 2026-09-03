#!/bin/bash
# QNX SDP 6.6 env for the PlayBook (SDP unzipped at /home/psyden/qnx660-master).
# Usage: source /home/psyden/playbook-dev/qnx-env.sh
export QNX_HOST=/home/psyden/qnx660-master/host/linux/x86
export QNX_TARGET=/home/psyden/qnx660-master/target/qnx6
export QNX_CONFIGURATION=/home/psyden/qnx660-master/.qnx
export MAKEFLAGS=-I$QNX_TARGET/usr/include
export PATH=$QNX_HOST/usr/bin:$PATH
export CC=arm-unknown-nto-qnx6.6.0eabi-gcc
export CXX=arm-unknown-nto-qnx6.6.0eabi-g++
