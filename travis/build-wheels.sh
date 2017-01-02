#!/bin/bash
set -e -x

rm -rf /opt/python/cp26-cp26m
rm -rf /opt/python/cp26-cp26mu

bash /io/travis/build-libunwind.sh

# Compile wheels
for PYBIN in /opt/python/*/bin; do
    ${PYBIN}/pip install -r /io/dev_requirements.txt
    ${PYBIN}/pip wheel /io/ -w wheels/
done

# Bundle external shared libraries into the wheels
for whl in wheels/*.whl; do
    if [[ "$whl" == *"vmprof"* ]]
    then
        auditwheel repair $whl -w /io/wheels/
    fi
done
