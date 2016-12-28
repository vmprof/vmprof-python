#!/bin/bash

set -e

if [[ "$(uname -s)" == 'Darwin' ]]; then
    source ~/.venv/bin/activate
fi

py.test vmprof/ -vrs
py.test jitlog/ -vrs

if [[ -n "TRAVIS_TAG" && "$BUILD_LINUX_WHEEL" == "1" ]]; then
    docker run --rm -v `pwd`:/io:Z $DOCKER_IMAGE $PRE_CMD bash /io/travis/build-wheels.sh;
fi
