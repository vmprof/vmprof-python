#!/bin/bash
set -e -x

bash /io/travis/build-libunwind.sh

# Compile wheels
for PYV in "cp27-cp27m" "cp27-cp27mu" "cp34-cp34m" "cp35-cp35m" "cp36-cp36m"
do
    /opt/python/${PYV}/bin/pip install -r /io/dev_requirements.txt
    /opt/python/${PYV}/bin/pip wheel /io/ -w wheels/
done

# Bundle external shared libraries into the wheels
for whl in wheels/*.whl; do
    if [[ "$whl" == *"vmprof"* ]]
    then
        auditwheel repair $whl -w /io/wheels/
    fi
done
