#!/bin/bash

# Read all the input!
cat > /dev/null

[ $# -ge 2 ] || {
    exit 0
}

option=$1
shift 1

case $option in
-P|--provides)
    echo -n "scl-package($1)"
    ;;
-R|--requires)
    for o in "$@"; do
        echo "$o"
    done
    ;;
esac
exit 0
