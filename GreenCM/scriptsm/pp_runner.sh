#! bin/bash

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
mkdir $resultsdir/plots_thresh
echo $3 > $resultsdir/desc.txt
fi

export SPINTOSLEEP=1500
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
backend[19]="asym-adpt-thresh"
backend[20]="dasym-spin"

config[1]="MOD_CM_POLICY=karma BO_POLICY=adpt CM_POLICY=backoff"
config[2]="MOD_CM_POLICY=karma BO_POLICY=spin CM_POLICY=backoff"
config[3]="MOD_CM_POLICY=karma BO_POLICY=sleep CM_POLICY=backoff"
config[4]="MOD_CM_POLICY=karma BO_POLICY=spin_pause CM_POLICY=backoff"
config[5]="MOD_CM_POLICY=karma BO_POLICY=asym_adpt CM_POLICY=backoff"
config[6]="MOD_CM_POLICY=karma BO_POLICY=asym_spin CM_POLICY=backoff"
config[7]="MOD_CM_POLICY=karma BO_POLICY=ddasym_adpt CM_POLICY=backoff"
config[8]="MOD_CM_POLICY=karma BO_POLICY=dasym_adpt CM_POLICY=backoff"
config[9]="MOD_CM_POLICY=karma BO_POLICY=spin CM_POLICY=suicide"
config[10]="MOD_CM_POLICY=karma BO_POLICY=spin CM_POLICY=delay"
config[11]="MOD_CM_POLICY=karma BO_POLICY=asym_sleep CM_POLICY=backoff"
config[12]="MOD_CM_POLICY=karma BO_POLICY=dasym_sleep CM_POLICY=backoff"
config[13]="MOD_CM_POLICY=karma BO_POLICY=ddasym_sleep CM_POLICY=backoff"
config[14]="MOD_CM_POLICY=karma BO_POLICY=spin CM_POLICY=modular"
config[15]="MOD_CM_POLICY=timestamp BO_POLICY=spin CM_POLICY=modular"
config[16]="MOD_CM_POLICY=aggressive BO_POLICY=spin CM_POLICY=modular"
config[17]="MOD_CM_POLICY=suicide BO_POLICY=spin CM_POLICY=modular"
config[18]="MOD_CM_POLICY=suicide BO_POLICY=adpt_thresh CM_POLICY=backoff"
config[19]="MOD_CM_POLICY=suicide BO_POLICY=asym_adpt_thresh CM_POLICY=backoff"
config[20]="MOD_CM_POLICY=suicide BO_POLICY=dasym_spin CM_POLICY=backoff"

benchmarks[1]="r"
benchmarks[2]="rw"
benchmarks[3]="w"

bStr[1]="r"
bStr[2]="rw"
bStr[3]="w"

wait_until_finish() {
    pid3=$1
    echo "process is $pid3"
    LIMIT=40
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

for be in 8
do
    for b in 2
    do
	#for f in 0
	#do
        #for x in 20000000 40000000
	#do
	export SPINTOSLEEP=1500
	export BETA=50
	#export FERRARIS=$f
        for t in 64
        do
		let ferraris=$t/4
                if [ $ferraris -le 1 ]; then
                        let ferraris=2;
                fi
		export FERRARIS=$ferraris
	    let con=$t
            cd $workspace/tinystm/abi;
            make clean;make ${config[$be]} gcc;
	    cd $workspace/mcd;
            make clean;./build_memcached.sh;
            for a in 1 2 3
            do
                echo "${bStr[$b]} | backend ${backend[$be]} | SPINTOSLEEP $x | FERRARIS $f | threads $t | attempt $a "
                ./memcached -p 20000 -t $t > $runsdir/${bStr[$b]}-${backend[$be]}-$t-$a.eng 2> $runsdir/${bStr[$b]}-${backend[$be]}-$t-$a.freq & 
                pid_server=$!
		echo "memcslap -F $workspace/scriptsm/memslap-${benchmarks[$b]}.cnf --time=30s -s 127.0.0.1:20000 -c $con -T $t -B > $runsdir/${bStr[$b]}-${backend[$be]}-$t-$a.data 2> $runsdir/${bStr[$b]}-${backend[$be]}-$t-$a.err"
                memcslap -F $workspace/scriptsm/memslap-${benchmarks[$b]}.cnf --time=30s -s 127.0.0.1:20000 -c $con -T $t -B > $runsdir/${bStr[$b]}-${backend[$be]}-$t-$a.data 2> $runsdir/${bStr[$b]}-${backend[$be]}-$t-$a.err &
                pid_client=$!
                wait_until_finish $pid_client; wait $pid_client; rc=$?
                if [[ $rc != 0 ]] ; then
                    echo "Error within: backend ${backend[$be]} | ${bStr[$b]} | SPINTOSLEEP $x | FERRARIS $f | threads $t | attempt $a" >> $runsdir/error.out
                fi
                kill -SIGINT $pid_server 
                wait $pid_server
           done
        done
    done
done
