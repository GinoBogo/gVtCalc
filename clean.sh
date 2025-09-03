#!/usr/bin/env bash
# Author: Gino Francesco Bogo

clear

path=$(
    cd "$(dirname "$0")"
    pwd -P
)

rm -rf $path/build 2>/dev/null

echo "... all cleaned."
