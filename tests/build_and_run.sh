#!/usr/bin/env bash

# sudo docker-compose down --rmi all -v --remove-orphans
# sudo docker-compose up --build -d
cd ..
sudo mkdir build
cd build
sudo cmake -DPACKAGE_TESTS=On .. ; make -j`nproc`
./tests/tests --reporter compact --success -d yes
sudo cd  ..
sudo cd tests
# sudo docker-compose down --rmi all -v --remove-orphans