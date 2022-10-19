#!/bin/bash

BASE=/paper149/


#
# Dependencies
#
echo "Installing dependencies..."
echo "tacas23" | sudo -S apt update && apt -y --no-install-recommends install \
        make \
        cmake \
        git \
        default-jre-headless \
        python3 \
        python3-distutils \
        uuid \
        uuid-dev \
        pkg-config \
        clang \
        libc++1 \
        libc++-dev \
        libc++abi1 \
        libc++abi-dev \
        zsh \
        texlive-latex-base \
        texlive-latex-extra \
        libpgf6
echo "tacas23" | sudo -S mkdir -p $BASE
echo "tacas23" | sudo -S chown tacas23:tacas23 $BASE
echo "tacas23" | sudo -S chmod a+rw $BASE
cd $BASE

#
# Z3
#
echo "Installing Z3..."
mkdir -p $BASE/z3_source
unzip z3-4.8.7.zip -d $BASE/z3_source/
mkdir -p $BASE/z3_source/build
cd $BASE/z3_source/build
CC=clang CXX=clang++ cmake .. -DCMAKE_INSTALL_PREFIX=/usr/
CC=clang CXX=clang++ make -j$((`nproc`+1))
echo "tacas23" | sudo -S make install

#
# Plankton & Krill
#
echo "Installing krill..."
mkdir -p $BASE/krill_source
unzip krill.zip -d $BASE/krill_source/
mkdir -p $BASE/krill_source/build
cp $BASE/krill_source/build
CC=clang CXX=clang++ cmake .. -DCMAKE_INSTALL_PREFIX=/ -DINSTALL_FOLDER=$BASE -DCMAKE_BUILD_TYPE=RELEASE
CC=clang CXX=clang++ make -j$((`nproc`+1))
make install

#
# Start
#
clear
cd $BASE
echo "Installation done!"
echo "Quick start evaluation: just run './eval.sh'"
