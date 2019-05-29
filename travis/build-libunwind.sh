#!/bin/bash
yum install -y libtool
cd
curl -O http://download.savannah.gnu.org/releases/libunwind/libunwind-1.2.tar.gz
tar xf libunwind-1.2.tar.gz
cd libunwind-1.2/
./configure
make install
