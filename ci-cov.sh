#!/bin/bash

if [ "$(uname -s)" == "Linux" ]; then
    python -m venv ccenv
    . ./ccenv/bin/activate || . ./ccenv/Scripts/activate
    pip install codecov
    python -m codecov -t "$1"
fi
