#!/bin/bash
yum install -y libtool
cd
git clone --depth 1 git://git.sv.gnu.org/libunwind.git
cd libunwind/
# what did you expect? this is centos 5 :), automake is a dinosaur
sed -i -e "s/LT_INIT/AC_PROG_LIBTOOL/" configure.ac
./autogen.sh 
make install
