#!/bin/bash

workspace=$1
if [ $4 == "no_random" ];then
resultsdir=$2
runsdir=$resultsdir/runs
else
resultsdir=$2/$RANDOM
mkdir -p $resultsdir/runs
runsdir=$resultsdir/runs
mkdir $resultsdir/plots
mkdir $resultsdir/summary
echo $3 > $resultsdir/desc.txt
fi


export FERRARIS=16
export THRESHOLD=60000
export STM_STATS=True
export LD_LIBRARY_PATH=/home/shady/x86_energy/build:$LD_LIBRARY_PATH

backend[1]="adpt-1500"
backend[2]="spin"
backend[3]="sleep"
backend[4]="spin-pause"
backend[5]="asym-adpt"
backend[6]="asym-spin"
backend[7]="ddasym-adpt"
backend[8]="dasym-adpt-bidim"
backend[9]="suicide"
backend[10]="delay"
backend[11]="asym-sleep"
backend[12]="dasym-sleep"
backend[13]="ddasym-sleep"
backend[14]="karma"
backend[15]="timestamp"
backend[16]="aggressive"
backend[17]="modsuicide"
backend[18]="adpt-thresh-stab-jmp1"


config[1]="MOD_CM_POLICY=karma BO_POLICY=adpt CM_POLICY=backoff"
config[2]="MOD_CM_POLICY=karma BO_POLICY=spin CM_POLICY=backoff"
config[3]="MOD_CM_POLICY=karma BO_POLICY=sleep CM_POLICY=backoff"
config[4]="BO_POLICY=spin_pause CM_POLICY=backoff"
config[5]="MOD_CM_POLICY=karma BO_POLICY=asym_adpt CM_POLICY=backoff"
config[6]="BO_POLICY=asym_spin CM_POLICY=backoff"
config[7]="BO_POLICY=ddasym_adpt CM_POLICY=backoff"
config[8]="MOD_CM_POLICY=karma BO_POLICY=dasym_adpt CM_POLICY=backoff"
config[9]="MOD_CM_POLICY=karma BO_POLICY=spin CM_POLICY=suicide"
config[10]="BO_POLICY=spin CM_POLICY=delay"
config[11]="BO_POLICY=asym_sleep CM_POLICY=backoff"
config[12]="BO_POLICY=dasym_sleep CM_POLICY=backoff"
config[13]="BO_POLICY=ddasym_sleep CM_POLICY=backoff"
config[14]="MOD_CM_POLICY=karma BO_POLICY=spin CM_POLICY=modular"
config[15]="MOD_CM_POLICY=timestamp BO_POLICY=spin CM_POLICY=modular"
config[16]="MOD_CM_POLICY=aggressive BO_POLICY=spin CM_POLICY=modular"
config[17]="MOD_CM_POLICY=suicide BO_POLICY=spin CM_POLICY=modular"
config[18]="MOD_CM_POLICY=karma BO_POLICY=adpt_thresh CM_POLICY=backoff"


benchmarks[1]="redblacktree"
benchmarks[2]="genomepp"
benchmarks[3]="intruderpp"
benchmarks[4]="kmeans"
benchmarks[5]="labyrinth"
benchmarks[6]="ssca2pp"
benchmarks[7]="vacationhpp"
benchmarks[8]="yadapp"
benchmarks[9]="array"
benchmarks[10]="bayes"
benchmarks[11]="genome"
benchmarks[12]="intruder"
benchmarks[13]="yada"
benchmarks[14]="intruderp"

executables[1]="redblacktree"
executables[2]="genome"
executables[3]="intruder"
executables[4]="kmeans"
executables[5]="labyrinth"
executables[6]="ssca2"
executables[7]="vacation"
executables[8]="yada"
executables[9]="array"
executables[10]="bayes"
executables[11]="genome"
executables[12]="intruder"
executables[13]="yada"
executables[14]="intruder"



params[1]="-d 30000000 -i 1048576 -r 1000000 -u 90 -n "
params[2]="-g16384 -s64 -n16777216 -t"
params[3]="-a10 -l128 -n262144 -s1 -t"
params[4]="-m15 -n15 -t0.00001 -i inputs/random-n65536-d32-c16.txt -p"
params[5]="-i inputs/random-x512-y512-z7-n512.txt -t"
params[6]="-s20 -i1.0 -u1.0 -l3 -p3 -t"
params[7]="-n4 -q60 -u90 -r1048576 -t4194304 -c"
params[8]="-a15 -i inputs/ttimeu1000000.2 -t"
params[9]="-s 1000 -o10000000 -i10 -c10 -w0 -t"
params[10]="-v32 -r4096 -n10 -p40 -i2 -e8 -s1"
params[11]="-g256 -s16 -n16384 -t"
params[12]="-a10 -l4 -n2048 -s1 -t"
params[13]="-a20 -i inputs/633.2 -t"
params[14]="-a10 -l16 -n4096 -s1 -t"

wait_until_finish() {
    pid3=$1
    echo "process is $pid3"
    LIMIT=90
    for ((j = 0; j < $LIMIT; ++j)); do
        kill -s 0 $pid3
        rc=$?
        if [[ $rc != 0 ]] ; then
            echo "returning"
            return;
        fi
        sleep 1s
    done
    kill -9 $pid3
}


benchmarks7[1]="-w r -t false -m false"
benchmarks7[2]="-w rw -t false -m false"
benchmarks7[3]="-w w -t false -m false"
benchmarks7[4]="-w r -t false -m true"
benchmarks7[5]="-w rw -t false -m true"
benchmarks7[6]="-w w -t false -m true"
benchmarks7[7]="-w r -t true -m false"
benchmarks7[8]="-w rw -t true -m false"
benchmarks7[9]="-w w -t true -m false"
benchmarks7[10]="-w r -t true -m true"
benchmarks7[11]="-w rw -t true -m true"
benchmarks7[12]="-w w -t true -m true"

bstr[1]="r-tf-mf"
bstr[2]="rw-tf-mf"
bstr[3]="w-tf-mf"
bstr[4]="r-tf-mt"
bstr[5]="rw-tf-mt"
bstr[6]="w-tf-mt"
bstr[7]="r-tt-mf"
bstr[8]="rw-tt-mf"
bstr[9]="w-tt-mf"
bstr[10]="r-tt-mt"
bstr[11]="rw-tt-mt"
bstr[12]="w-tt-mt"

#rttmt 64 asym-adpt
#for x in 500 5000 10000 50000 100000 300000 500000 1000000 40000000 
#do
#for f in 0 2 4 8 16
#do
export SPINTOSLEEP=1500
export BETA=50
#export FERRARIS=$f
for t in 64
do
for c in 8
do
    cd $workspace;
    cd tinystm;
    make clean;
    make ${config[$c]};
    cd $workspace;
    echo "building ${backend[$c]}"
    cd stmbench7;
    make clean;
    make PROFILE=fast STM=tinySTM;
    cd $workspace;
    for b in 5
    do
        #for t in 48 64
        #do
               let ferraris=$t/4
               if [ $ferraris -le 1 ]; then
                       let ferraris=2;
               fi 
               export FERRARIS=$ferraris

            for a in 1 2 3
            do
                echo "${bstr[$b]} | ${backend[$c]} | threads $t | ferraris $f | attempt $a"
                ./stmbench7/sb7_tt -i false -s b -h false -r false ${benchmarks7[$b]} -d 60000 -n $t > $runsdir/${bstr[$b]}-${backend[$c]}-$t-$a.data  2> $runsdir/${bstr[$b]}-${backend[$c]}-$t-$a.out
                rc=$?
                if [[ $rc != 0 ]] ; then
                    echo "Error within: ${backend[$c]} | ${bstr[$b]} | threads $t | attempt $a" >> $runsdir/error.out
                fi
            done
        done
    done
done
#done
#done


echo $resultsdir
