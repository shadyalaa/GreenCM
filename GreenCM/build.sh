#!/bin/sh

FOLDERS="genome  intruder  kmeans  labyrinth  ssca2  vacation  yada"

rm lib/*.o || true

name=$1
prob=$2

cp backend-$name/Defines.common.mk common/
cp backend-$name/Makefile common/
cp backend-$name/thread.h lib/
cp backend-$name/thread.c lib/
cp backend-$name/tm.h lib/

for F in $FOLDERS
do
    cd $F
    rm *.o || true
    rm $F
    make -f Makefile PROB="-DFALLBACK_PROB=$prob"
    rc=$?
    if [[ $rc != 0 ]] ; then
	echo ""
        echo "=================================== ERROR BUILDING $F $name $prob ===================================="
	echo ""
        exit 1
    fi
    cd ..
done
