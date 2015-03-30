scl()
{
if [ "$1" = "load" -o "$1" = "unload" ]; then
# It is possible that function module is not declared in time of this
# declaration so eval is used instead of direct calling of function module
    eval "module $@"
else
   /usr/bin/scl "$@"
fi
}

shell=`/bin/basename \`/bin/ps -p $$ -ocomm=\``
[ "$shell" = "bash" ] && export -f scl # export -f works only in bash

MODULESHOME=/usr/share/Modules
export MODULESHOME

if [ "${MODULEPATH:-}" = "" ]; then
   MODULEPATH=`sed -n 's/[   #].*$//; /./H; $ { x; s/^\n//; s/\n/:/g; p; }' ${MODULESHOME}/init/.modulespath`
fi

MODULEPATH=/etc/scl/modulefiles:$MODULEPATH

export MODULEPATH


