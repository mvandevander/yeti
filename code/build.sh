#!/bin/bash
mkdir -p ../build
pushd ../build
clang ../code/linux_yeti.cpp -g -o0 -Wall -lxcb -o"linux_yeti"