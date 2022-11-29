#!/bin/bash

set -o errexit

python3 -m venv env
. ./env/bin/activate || . ./env/Scripts/activate
make requirements
make ctest
make pybuild
make pytest
