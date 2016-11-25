#!/bin/bash
set -e -x

# Install a system package required by our library
#yum install -y atlas-devel
rm -rf /opt/python/cp26-cp26m
rm -rf /opt/python/cp26-cp26mu

bash /io/travis/build-libunwind.sh

# Compile wheels
for PYBIN in /opt/python/*/bin; do
    ${PYBIN}/pip wheel /io/ -w wheels/
done

# Bundle external shared libraries into the wheels
for whl in wheels/*.whl; do
    auditwheel repair $whl -w /io/wheels/
done

