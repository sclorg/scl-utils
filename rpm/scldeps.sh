#!/bin/bash

[ $# -ge 2 ] || {
    cat > /dev/null
    exit 0
}

option=$1
shift 1

case $option in
-P|--provides)
    echo -n "scl-package($1)"
    ;;
-R|--requires)
    for o in $@; do
        echo "$o"
    done
    ;;
esac
exit 0
