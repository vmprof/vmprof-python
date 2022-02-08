#!/bin/bash

set -e

if [[ "$(uname -s)" == 'Darwin' ]]; then
    source ~/.venv/bin/activate
fi

if [[ "$(uname -m)" == 'ppc64le' ]]; then
    # Disabled due to memory restrictions on Travis ppc64le CI
    EXTRA_OPTS='-k not test_basic and not test_read_bit_by_bit and not test_enable_disable and not test_start_end_time and not test_nested_call and not test_line_profiling and not test_vmprof_show'
fi

py.test vmprof/ -vrs "$EXTRA_OPTS"
py.test jitlog/ -vrs

if [[ -n "$TRAVIS_TAG" ]]; then

    if [[ "$MAC_WHEEL" == "1" ]]; then
        bash travis/upload-artifact.sh
    fi

    if [[ "$BUILD_LINUX_WHEEL" == "1" ]]; then
        docker pull quay.io/pypa/manylinux1_x86_64
        docker pull quay.io/pypa/manylinux1_i686
        docker run --rm -v `pwd`:/io:Z quay.io/pypa/manylinux1_x86_64 bash /io/travis/build-wheels.sh;
        docker run --rm -v `pwd`:/io:Z quay.io/pypa/manylinux1_i686 linux32 bash /io/travis/build-wheels.sh;
        bash travis/upload-artifact.sh
    fi

fi
