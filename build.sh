#!/usr/bin/env bash

base_dir=$(dirname "$0")

PYTHONDONTWRITEBYTECODE=1 exec python2 "$base_dir/build.py" "$@"
