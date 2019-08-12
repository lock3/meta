#!/bin/bash
#set -x
set -e

if [[ $# -ne 1 ]]; then
  echo "usage: $0 path-to-clang"
  exit 1
fi

CLANG=$1
FLAGS="-Xclang -verify -fsyntax-only"

function test_libstdcpp() {
  INSTALL_DIR=`pwd`/install-libstdc++/include/c++/$1.$2.$3
  if [[ ! -d $INSTALL_DIR ]]; then
    mkdir -p gcc
    cd gcc
    FILE=gcc-$1_$2_$3-release
    wget -N https://github.com/gcc-mirror/gcc/archive/$FILE.tar.gz
    test -d gcc-$FILE || tar -xzf $FILE.tar.gz
    mkdir -p build-$FILE
    cd build-$FILE
    test -f Makefile || CC=gcc CXX=g++ ../gcc-$FILE/libstdc++-v3/configure --disable-multilib --disable-nls --prefix=`pwd`/../../install-libstdc++
    make install-data
    cd ..
    cd ..
  fi
  echo "Testing libstdc++ $1.$2.$3"
  $CLANG -std=c++17 $FLAGS lifetime-attr-test.cpp -nostdlibinc -I$INSTALL_DIR -I/usr/include || {
    echo $CLANG -std=c++17 $FLAGS lifetime-attr-test.cpp -nostdlibinc -I$INSTALL_DIR -I/usr/include
    #$CLANG -std=c++17 -Xclang -ast-dump -fsyntax-only lifetime-attr-test.cpp -nostdlibinc -I$INSTALL_DIR -I/usr/include
  }
}

function test_libcpp() {
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
  echo "Testing libc++ $1.$2.$3"
  $CLANG -std=c++17 $FLAGS lifetime-attr-test.cpp -nostdlibinc -I$INSTALL_DIR -I/usr/include || true
 # -Xclang -ast-dump
}

function test_msvc() {
  if [[ ! -f $1/include/vector ]]; then
    echo "Skipping MSVC $1 because files $1/include/vector and/or ucrt/corecrt.h do not exist."
    echo "Copy their directories from a Window installation."
    return
  fi

  echo "Testing MSVC $1"
  $CLANG-cl /std:c++latest $FLAGS lifetime-attr-test.cpp -imsvc $1/include/ -imsvc ucrt || true
}


test_msvc 14.21.27702
test_msvc 14.20.27508
test_msvc 14.16.27023
test_msvc VC_14

#test_libcpp 6 0 1 https://github.com/llvm/llvm-project/archive/llvmorg-6.0.1.tar.gz
#test_libcpp 7 0 1
test_libcpp 7 1 0 https://github.com/llvm/llvm-project/releases/download/llvmorg-7.1.0/libcxx-7.1.0.src.tar.xz
test_libcpp 8 0 1rc2 https://github.com/llvm/llvm-project/releases/download/llvmorg-8.0.1-rc2/libcxx-8.0.1rc2.src.tar.xz


# Incompatible with clang, see https://bugzilla.redhat.com/show_bug.cgi?id=1129899
#test_libstdcpp 4 4 7

#archive does not include libstdc++
#test_libstdcpp 4 5 2
test_libstdcpp 4 6 4

# Fails building due to missing bits/gthr-default.h
#test_libstdcpp 4 7 3

test_libstdcpp 4 8 5
test_libstdcpp 4 9 4
test_libstdcpp 5 4 0
test_libstdcpp 6 5 0
test_libstdcpp 7 3 0
test_libstdcpp 8 3 0
test_libstdcpp 9 1 0


#$CLANG -std=c++17 -fsyntax-only lifetime-attr-test.cpp -nostdlibinc -Iinstall-libstdc++/include/c++/9.1.0 -I/usr/include

#$CLANG -std=c++17 -fsyntax-only lifetime-attr-test.cpp -I/usr/include/c++/5.5.0 -I/usr/include/x86_64-linux-gnu/c++/5.5.0 -I/usr/include -nostdlibinc
#$CLANG -fsyntax-only lifetime-attr-test.cpp -I/usr/include/c++/6.5.0 -nostdlibinc
#$CLANG -std=c++17 -fsyntax-only lifetime-attr-test.cpp -I/usr/include/c++/7.4.0 -I/usr/include/x86_64-linux-gnu/c++/7.4.0 -I/usr/include -nostdlibinc

#$CLANG -std=c++17 -fsyntax-only lifetime-attr-test.cpp -I libcxx/include -I /usr/include -nostdlibinc
#$CLANG -fsyntax-only lifetime-attr-test.cpp -I/usr/include/c++/8 -nostdlibinc
