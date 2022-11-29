#!/bin/bash

set -o errexit

if [ "$(uname -s)" == "Linux" ]; then
    sudo apt install -y valgrind
fi

make venv
. ./venv/bin/activate || . ./venv/Scripts/activate
make requirements
make ctest
make pybuild
make pytest
