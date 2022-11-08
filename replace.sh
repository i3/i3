#!/bin/sh

git pull --autostash
cd build
ninja

i3path=$(which i3)
sudo mv $i3path $i3path.old
sudo cp i3 $i3path
