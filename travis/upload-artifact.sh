#!/bin/bash

pip install virtualenv

virtualenv venv
source venv/bin/activate

pip install twine

python setup.py sdist

VERSION="$(python setup.py --version)"

twine upload -u $PYPI_USERNAME -p $PYPI_PASSWORD vmprof-$(VERSION).tar.gz
twine upload -u $PYPI_USERNAME -p $PYPI_PASSWORD vmprof-$(VERSION)*.whl
