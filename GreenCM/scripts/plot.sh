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
configs = "aggressive timestamp karma adpt asym-adpt dasym-adpt"
titles = "aggressive timestamp karma hybrid ferraris ferraris-t ferraris-tt"

#configs = "adpt spin sleep asym-sleep dasym-sleep"
#titles = "spin sleep ferraris ferraris-t ferraris-tt"

set output '$1/$2-power_t.png'
set ylabel "total power"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 7:(\$7-\$12):(\$7+\$12):xtic(1) title word(titles,i)

set output '$1/$2-edp.png'
set ylabel "EDP"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 9:(\$9-\$14):(\$9+\$14):xtic(1) title word(titles,i)

set output '$1/$2-edp_t.png'
set ylabel "EDP total"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 10:(\$10-\$15):(\$10+\$15):xtic(1) title word(titles,i)


set output '$1/$2-power.png'
set ylabel "energy"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 6:(\$6-\$11):(\$6+\$11):xtic(1) title word(titles,i)


set output '$1/$2-time.png'
set ylabel "time"
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 8:(\$8-\$13):(\$8+\$13):xtic(1) title word(titles,i)


set output '$1/$2-retries.png'
set ylabel "retries"
set logscale y 10
set autoscale
set style histogram
plot for [i=1:words(configs)] '$3-'.word(configs, i) every::1 using 5:xtic(1) title word(configs,i)




EOF
