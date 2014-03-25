#!/bin/bash

[ $# -ge 2 ] || {
    cat > /dev/null
    exit 0
}

case $1 in
-P|--provides)
    echo -n "scl-package($2)"
    ;;
-R|--requires)
    echo -n "$2"
    ;;
esac
exit 0
