#!/bin/sh



gnuplot << EOF


reset

set terminal png
set style histogram errorbars gap 2 lw 1
set style data histogram
set grid
set xlabel "threads"

set autoscale
set key center top
set yrange [0:]

set title "$2"


set style fill solid

configs = "adpt asym-adpt dasym-adpt"
titles = "hybrid ferraris ferraris-t ferraris-tt"

#configs = "adpt spin sleep asym-sleep dasym-sleep"
#titles = "spin sleep ferraris ferraris-t ferraris-tt"

set output '$1/$2-power_t.png'
set ylabel "total power"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 7:(\$7-\$13):(\$7+\$13):xtic(1) title word(titles,i)

set output '$1/$2-edp.png'
set ylabel "EDP"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 9:(\$9-\$15):(\$9+\$15):xtic(1) title word(titles,i)

set output '$1/$2-edp_t.png'
set ylabel "EDP total"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 10:(\$10-\$16):(\$10+\$16):xtic(1) title word(titles,i)


set output '$1/$2-power.png'
set ylabel "energy"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 6:(\$6-\$12):(\$6+\$12):xtic(1) title word(titles,i)


set output '$1/$2-commits.png'
set ylabel "throughput"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 2:(\$2-\$14):(\$2+\$14):xtic(1) title word(titles,i)

set style histogram

set output '$1/$2-time.png'
set ylabel "time"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 8:xtic(1) title word(titles,i)

set output '$1/$2-rtime.png'
set ylabel "rtime"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 11:xtic(1) title word(titles,i)


EOF
