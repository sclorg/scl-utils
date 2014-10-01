function scl()
{
local CMD=$1
if [ "$CMD" = "load" -o "$CMD" = "unload" ]; then
    module $@
else
   /usr/bin/scl $@
fi
}
export -f scl
