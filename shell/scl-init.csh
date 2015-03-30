alias scl 'source /etc/scl/func_scl.csh'

setenv MODULESHOME /usr/share/Modules

if (! $?MODULEPATH ) then
    setenv MODULEPATH `sed -n 's/[    #].*$//; /./H; $ { x; s/^\n//; s/\n/:/g; p; }' ${MODULESHOME}/init/.modulespath`
endif

setenv MODULEPATH /etc/scl/modulefiles:$MODULEPATH
