#!/usr/bin/bash
. /usr/share/beakerlib/beakerlib.sh

TEST="Collections test"


rpmdir=`pwd`/rpmdir

rlJournalStart
    rlPhaseStartTest "building collections"
        for col_srpm_dir in SRPMS/*; do
            rm -rf $rpmdir
            mkdir $rpmdir

            colname=`basename $col_srpm_dir`
            srpms=$col_srpm_dir/*

            rlLog "Collection: $colname"

            rpmbuild --define "_topdir $rpmdir" --define "scl $colname" --rebuild $srpms
            rlAssert0 "Check if it is possible to build collection" $?

            runtime_package=`find $rpmdir/RPMS | grep "/${colname}-runtime"`
            rpm -qp --provides  $runtime_package | grep "scl-package($colname)"
            rlAssert0 "Check if runtime packages provides scl-package($colname)" $?

            non_runtime_packages=`find $rpmdir/RPMS -type f | grep -v "/${colname}-runtime"`
            echo $non_runtime_packages;
            for package in $non_runtime_packages; do
                rpm -qpR $package | grep ${colname}-runtime
                rlAssert0 "Check if `basename $package` requires ${colname}-runtime" $?
            done
        done
    rlPhaseEnd

    rlPhaseStartTest "collection runtime"
        rpm -q test555-runtime && yum erase -y test555-runtime
        yum install -y RPMS/test555-runtime-1-1.x86_64.rpm
        rlAssert0 "Check if it is possible to install collection test555" $?

        scl list-collections | grep test555
        rlAssert0 "Check if collection test555 is in list of collections" $?

        rm -rf /tmp/scl
        mkdir /tmp/scl
        scl deregister -f test555
        rlAssert0 "Check if it is possilbe to deregister collection test555" $?

        test -f /tmp/scl/deregister
        rlAssert0 "Check if deregister script was successfully executed" $?

        scl register /opt/rh/test555
        rlAssert0 "Check if it is possilbe to register collection test555" $?

        test -f /tmp/scl/register
        rlAssert0 "Check if register script was successfully executed" $?

    rlPhaseEnd

rlJournalEnd

rlJournalPrintText

rlGetTestState


