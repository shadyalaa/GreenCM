#!/bin/sh

scriptsfolder=$1

backend[1]="adpt"
backend[2]="spin"
backend[3]="sleep"
backend[4]="spin-pause"

benchmarks[1]="redblacktree"
benchmarks[2]="genome"
benchmarks[3]="intruder"
benchmarks[4]="kmeans"
benchmarks[5]="labyrinth"
benchmarks[6]="ssca2"
benchmarks[7]="vacation"
benchmarks[8]="yada"
benchmarks[9]="array"

for c in 1 2 3 4
do
        for b in 1 3 5 7 8 9
        do
		echo "collecting results for ${benchmarks[$b]}-${backend[$c]}"
		python $scriptsfolder/results_analyzer.py ${benchmarks[$b]}-${backend[$c]} $2 > $3/${benchmarks[$b]}-${backend[$c]}
        done
done

exit 0



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


    cd stmbench7-orig;
    make clean;
    make PROFILE=fast STM=tinystm;
    for b in 1 2 3 4 5 6 7 8 9 10 11 12
    do
        for t in 1 2 4 8 16 24 32 48 64
        do
            for a in 1 2 3 #4 5 #4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
            do
                echo "${bstr[$b]} | ${backend[$c]} | threads $t | attempt $a"
                ./sb7_tt -i false -s b -h false -r false ${benchmarks[$b]} -d 20000 -n $t > $2/${bstr[$b]}-$t-$a.data  2> $2/${bstr[$b]}-$t-$a.out
                rc=$?
                if [[ $rc != 0 ]] ; then
                    echo "Error within: ${backend[$c]} | ${bstr[$b]} | threads $t | attempt $a" >> $2/error.out
                fi
            done
        done
    done
