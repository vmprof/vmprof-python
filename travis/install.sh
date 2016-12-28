#!/bin/bash

set -e

if [[ "$(uname -s)" == 'Darwin' ]]; then
    brew update || brew update
    git clone --depth 1 https://github.com/yyuu/pyenv.git ~/.pyenv
    PYENV_ROOT="$HOME/.pyenv"
    PATH="$PYENV_ROOT/bin:$PATH"
    eval "$(pyenv init -)"
    pyenv install $PYENV
    pyenv global $PYENV
    pyenv rehash
    echo "python version $(python --version)"
    python -m pip install --user virtualenv

    python -m virtualenv ~/.venv
    source ~/.venv/bin/activate
else
    if [[ -n "TRAVIS_TAG" && "$BUILD_LINUX_WHEEL" == "1" ]]; then
        docker pull $DOCKER_IMAGE;
    fi
fi

pip install .
pip install -r test_requirements.txt
