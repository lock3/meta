#!/bin/bash
#set -x
set -e

if [[ $# -ne 1 ]]; then
  echo "usage: $0 path-to-clang"
  exit 1
fi

if [[ ! -f lifetime-attr-test.cpp ]]; then
  echo "ERROR: First change into the directory of this script."
  exit 1
fi

FAILED=0

CLANG=$1
FLAGS="-Xclang -verify -fsyntax-only"

function test_libstdcpp() {
  INSTALL_DIR=`pwd`/install-libstdc++/include/c++/$1.$2.$3
  if [[ ! -d $INSTALL_DIR ]]; then
    mkdir -p gcc
    cd gcc
    FILE=gcc-$1.$2.$3
    wget -N https://github.com/gcc-mirror/gcc/archive/releases/$FILE.tar.gz
    test -d gcc-$FILE || tar -xzf $FILE.tar.gz
    mkdir -p build-$FILE
    cd build-$FILE
    test -f Makefile || CC=gcc CXX=g++ ../gcc-releases-$FILE/libstdc++-v3/configure --disable-multilib --disable-nls --prefix=`pwd`/../../install-libstdc++
    make install-data
    cd ..
    cd ..
  fi
  echo "Testing libstdc++ $1.$2.$3"
  shift; shift; shift;
  echo "  with $@"
  $CLANG -std=c++2a $FLAGS "$@" -nostdlibinc -I$INSTALL_DIR -I/usr/include -I/usr/include/x86_64-linux-gnu/ || {
    FAILED=1
    echo $CLANG -std=c++2a $FLAGS "$@" -nostdlibinc -I$INSTALL_DIR -I/usr/include -I/usr/include/x86_64-linux-gnu/
  }
}

function get_libcpp() {
  INSTALL_DIR=install-libc++/$1.$2.$3/include/c++/v1
  if [[ ! -d $INSTALL_DIR ]]; then
    mkdir -p libc++
    cd libc++
    FILE=libcxx-$1.$2.$3
    wget -N $4
    #/llvm-project-llvmorg-6.0.1/libcxx/
    test -d build-$FILE.src || tar -xf $FILE.src.tar.xz
    mkdir -p build-$FILE.src
    cd build-$FILE.src
    cmake ../$FILE.src -DCMAKE_INSTALL_PREFIX=`pwd`/../../install-libc++/$1.$2.$3
    make install-cxx-headers
    cd ..
    cd ..
  fi
}

function test_libcpp() {
  INSTALL_DIR=install-libc++/$1.$2.$3/include/c++/v1
  echo "Testing libc++ $1.$2.$3"
  shift; shift; shift;
  echo "  with $@"
  $CLANG -std=c++2a $FLAGS "$@" -nostdlibinc -I$INSTALL_DIR -I/usr/include -I/usr/include/x86_64-linux-gnu/ || {
    FAILED=1
    echo $CLANG -std=c++2a $FLAGS "$@" -nostdlibinc -I$INSTALL_DIR -I/usr/include -I/usr/include/x86_64-linux-gnu/
  }
}

function test_msvc() {
  if [[ ! -f $1/include/vector ]]; then
    echo "Skipping MSVC $1 because files $1/include/vector and/or ucrt/corecrt.h do not exist."
    echo "Copy their directories from a Window installation."
    return
  fi

  echo "Testing MSVC $1"
  INSTALL_DIR="$1"
  shift
  echo "  with $@"
  $CLANG-cl /std:c++latest $FLAGS "$@" -imsvc $INSTALL_DIR/include/ -imsvc ucrt || {
    FAILED=1
    echo $CLANG-cl /std:c++latest $FLAGS "$@" -imsvc $INSTALL_DIR/include/ -imsvc ucrt
  }
}

function get_range_v3() {
  if [ -d range-v3 ]; then
    return
  fi
  git clone https://github.com/ericniebler/range-v3.git
  cd range-v3
  git checkout c50d5b32
  cd ..
}

get_range_v3
get_libcpp 7 1 0 https://github.com/llvm/llvm-project/releases/download/llvmorg-7.1.0/libcxx-7.1.0.src.tar.xz
get_libcpp 8 0 1 https://github.com/llvm/llvm-project/releases/download/llvmorg-8.0.1/libcxx-8.0.1.src.tar.xz


test_msvc 14.21.27702 lifetime-attr-test.cpp
test_msvc 14.20.27508 lifetime-attr-test.cpp
test_msvc 14.16.27023 lifetime-attr-test.cpp
test_msvc VC_14 lifetime-attr-test.cpp

#test_libcpp 6 0 1 https://github.com/llvm/llvm-project/archive/llvmorg-6.0.1.tar.gz
#test_libcpp 7 0 1
test_libcpp 7 1 0 lifetime-attr-test.cpp
test_libcpp 8 0 1 lifetime-attr-test.cpp

test_libstdcpp 4 8 5 lifetime-attr-test.cpp
test_libstdcpp 4 9 4 lifetime-attr-test.cpp
test_libstdcpp 5 4 0 lifetime-attr-test.cpp
test_libstdcpp 6 5 0 lifetime-attr-test.cpp
test_libstdcpp 7 3 0 lifetime-attr-test.cpp
test_libstdcpp 8 3 0 lifetime-attr-test.cpp
# libstdc++ 9.1.0 is incompatible with clang, see https://bugzilla.redhat.com/show_bug.cgi?id=1719103
test_libstdcpp 9 2 0 lifetime-attr-test.cpp

# Crashes, see https://github.com/mgehre/llvm-project/issues/31
# test_msvc 14.21.27702 warn-lifetime-godbolt.cpp /EHa /I range-v3/include -Wno-return-stack-address -Wno-dangling -Wlifetime
test_libcpp 8 0 1  warn-lifetime-godbolt.cpp -isystem range-v3/include -Wno-return-stack-address -Wno-dangling -Wlifetime
test_libstdcpp 9 2 0  warn-lifetime-godbolt.cpp -isystem range-v3/include -Wno-return-stack-address -Wno-dangling -Wlifetime

if [[ "$FAILED" == "1" ]]; then
  exit 1
fi
