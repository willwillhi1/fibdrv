reset
set ylabel 'time(ns)'
set title 'system call overhead'
set key left top
set term png enhanced font 'Verdana,10'
set output 'exp2.png'
plot [2:93][:] \
'experiment/exp2_plot_input' using 1:2 with linespoints linewidth 2 title "user space",\
'' using 1:3 with linespoints linewidth 2 title "kernel space",\
'' using 1:4 with linespoints linewidth 2 title "system call overhead",\