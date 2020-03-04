#!/bin/bash

set -e

if [[ "$(uname -s)" == 'Darwin' ]]; then
    git clone --depth 1 https://github.com/yyuu/pyenv.git ~/.pyenv
    PYENV_ROOT="$HOME/.pyenv"
    PATH="$PYENV_ROOT/bin:$PATH"
    eval "$(pyenv init -)"
    pyenv install $PYENV
    pyenv global $PYENV
    pyenv rehash
    echo "python version $(python --version)"
    # these are noisy, turn off echo
    python -m pip install --user virtualenv

    python -m virtualenv ~/.venv
    source ~/.venv/bin/activate
fi

pip install .
pip install --upgrade -r test_requirements.txt
