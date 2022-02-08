#!/bin/bash
set -ex
yum install -y elfutils-libelf-devel libdwarf-devel libtool
cd
curl -L -O http://download.savannah.nongnu.org/releases/libunwind/libunwind-1.5.0.tar.gz
tar xf libunwind-1.5.0.tar.gz
cd libunwind-1.5.0/
autoreconf -i
./configure
make
make install
