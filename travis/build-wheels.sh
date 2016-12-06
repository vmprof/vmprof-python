#!/bin/bash
set -e -x

rm -rf /opt/cp26-cp26m /opt/cp26-cp26mu

# Install a system package required by our library
# yum install -y atlas-devel

# Compile wheels
for PYBIN in /opt/python/*/bin; do
    ${PYBIN}/pip install -r /io/dev-requirements.txt
    ${PYBIN}/pip wheel /io/ -w wheels
done

# Bundle external shared libraries into the wheels
for whl in wheelhouse/*.whl; do
    auditwheel repair $whl -w /io/wheels/
done

# Install packages and test
# for PYBIN in /opt/python/*/bin/; do
#     ${PYBIN}/pip install vmprof --no-index -f /io/wheels
#     (cd $HOME; ${PYBIN}/nosetests pymanylinuxdemo)
# done
