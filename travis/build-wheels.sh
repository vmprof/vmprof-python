#!/bin/bash
set -e -x

# remove the wheels that should not be built (we do not support Python 2.6)
rm -rf /opt/python/cp26-cp26m /opt/python/cp26-cp26mu

# Install a system package required by our library
# yum install -y atlas-devel

# Compile wheels
for PYBIN in /opt/python/*/bin; do
    ${PYBIN}/pip install -r /io/dev_requirements.txt
    ${PYBIN}/pip wheel /io/ -w wheels
done

# Bundle external shared libraries into the wheels
for whl in wheels/*.whl; do
    auditwheel repair $whl -w /io/wheels/
done

# Install packages and test
# for PYBIN in /opt/python/*/bin/; do
#     ${PYBIN}/pip install vmprof --no-index -f /io/wheels
#     (cd $HOME; ${PYBIN}/nosetests pymanylinuxdemo)
# done
