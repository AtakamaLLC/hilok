#!/bin/bash

set -o errexit

if [ "$(uname -s)" == "Linux" ]; then
    sudo apt install -y valgrind
fi

python3 -m venv env
. ./env/bin/activate || . ./env/Scripts/activate
make requirements
make ctest
make pybuild
make pytest
