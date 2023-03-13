reset
set ylabel 'time(ns)'
set title 'with and w/o clz'
set key left top
set term png enhanced font 'Verdana,10'
set output 'exp1.png'
plot [2:93][:] \
'experiment/exp1_plot_input' using 1:2 with linespoints linewidth 2 title "fast doubling w/o clz",\
'' using 1:3 with linespoints linewidth 2 title "fast doubling with clz",\