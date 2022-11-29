#!/bin/bash

python -m virtualenv ccenv
. ./ccenv/bin/activate || . ./ccenv/Scripts/activate
pip install codecov
python -m codecov -t "$1"
