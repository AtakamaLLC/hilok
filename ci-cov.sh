#!/bin/bash

python -m venv ccenv
. ./ccenv/bin/activate || . ./ccenv/Scripts/activate
pip install codecov
python -m codecov -t "$1"
