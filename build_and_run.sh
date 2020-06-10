#!/usr/bin/env bash

cd build
sudo cmake -DPACKAGE_TESTS=Off .. ; make
cd ..
rm -rf tfs
cp build/tfs ./
./tfs