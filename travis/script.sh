#!/bin/bash

set -e

if [[ "$(uname -s)" == 'Darwin' ]]; then
    source ~/.venv/bin/activate
fi

py.test vmprof/ -vrs
py.test jitlog/ -vrs

if [[ -n "$TRAVIS_TAG" && "$BUILD_LINUX_WHEEL" == "1" ]]; then
    docker pull quay.io/pypa/manylinux1_x86_64
    docker pull quay.io/pypa/manylinux1_i686
    docker run --rm -v `pwd`:/io:Z quay.io/pypa/manylinux1_x86_64 bash /io/travis/build-wheels.sh;
    docker run --rm -v `pwd`:/io:Z quay.io/pypa/manylinux1_i686 linux32 bash /io/travis/build-wheels.sh;
    bash travis/upload-artifact.sh
fi
