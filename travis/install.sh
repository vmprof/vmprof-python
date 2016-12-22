#!/bin/bash

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

    pip install pytest
    python -m virtualenv ~/.venv
    source ~/.venv/bin/activate
    find ~/ -name 'py.test'
fi

pip install .
pip install -r test_requirements.txt
