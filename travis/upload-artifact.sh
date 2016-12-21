#!/bin/bash

pip install virtualenv

virtualenv venv
source venv/bin/activate

pip install twine


VERSION="$(python setup.py --version)"

if [ -n "$UPLOAD_SDIST" ]; then
    echo "uploading source distribution"
    python setup.py sdist
    twine upload -u $PYPI_USERNAME -p $PYPI_PASSWORD vmprof-$(VERSION).tar.gz
fi

if [ -n "$BUILD_LINUX_WHEEL" ]; then
    echo "uploading built wheels"
    twine upload -u $PYPI_USERNAME -p $PYPI_PASSWORD vmprof-$(VERSION)*.whl
fi

