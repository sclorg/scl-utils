if ( "$1" == "load" || "$1" == "unload" ) then
# It is possible that function module is not declared in time of this
# declaration so eval is used instead of direct calling of function module
    eval "module $*:q"
else
    /usr/bin/scl $*:q
endif
