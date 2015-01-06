function scl()
{
local CMD=$1
if [ "$CMD" = "load" -o "$CMD" = "unload" ]; then
# It is possible that function module is not declared in time of this
# declaration so eval is used instead of direct calling of function module
    eval "module $@"
else
   /usr/bin/scl $@
fi
}
export -f scl

MODULESHOME=/usr/share/Modules
export MODULESHOME

if [ "${MODULEPATH:-}" = "" ]; then
   MODULEPATH=`sed -n 's/[   #].*$//; /./H; $ { x; s/^\n//; s/\n/:/g; p; }' ${MODULESHOME}/init/.modulespath`
fi

MODULEPATH=/etc/scl/modulefiles:$MODULEPATH

export MODULEPATH


