set title "Distribution of scores over generations"
set xlabel "# of generations"
set ylabel "Score"
plot "data.txt" using 1:2 with lines lc "black" title "100th percentile", \
    "data.txt" using 1:3 with lines lc "grey50" title "99th percentile", \
    "data.txt" using 1:4 with lines lc "grey50" title "98th percentile", \
    "data.txt" using 1:5 with lines lc "grey50" title "97th percentile", \
    "data.txt" using 1:6 with lines lc "grey50" title "96th percentile", \
    "data.txt" using 1:7 with lines lc "grey50" title "95th percentile", \
    "data.txt" using 1:8 with lines lc "grey50" title "94th percentile", \
    "data.txt" using 1:9 with lines lc "grey50" title "93th percentile", \
    "data.txt" using 1:10 with lines lc "grey50" title "92nd percentile", \
    "data.txt" using 1:11 with lines lc "grey50" title "91st percentile", \
    "data.txt" using 1:12 with lines lc "black" title "90th percentile", \
    "data.txt" using 1:13 with lines lc "black" title "80th percentile", \
    "data.txt" using 1:14 with lines lc "black" title "70th percentile", \
    "data.txt" using 1:15 with lines lc "black" title "60th percentile", \
    "data.txt" using 1:16 with lines lc "red" title "50th percentile", \
    "data.txt" using 1:17 with lines lc "black" title "40th percentile", \
    "data.txt" using 1:18 with lines lc "black" title "30th percentile", \
    "data.txt" using 1:19 with lines lc "black" title "20th percentile", \
    "data.txt" using 1:20 with lines lc "black" title "10th percentile", \
    "data.txt" using 1:21 with lines lc "grey50" title "9th percentile", \
    "data.txt" using 1:22 with lines lc "grey50" title "8th percentile", \
    "data.txt" using 1:23 with lines lc "grey50" title "7th percentile", \
    "data.txt" using 1:24 with lines lc "grey50" title "6th percentile", \
    "data.txt" using 1:25 with lines lc "grey50" title "5th percentile", \
    "data.txt" using 1:26 with lines lc "grey50" title "4th percentile", \
    "data.txt" using 1:27 with lines lc "grey50" title "3th percentile", \
    "data.txt" using 1:28 with lines lc "grey50" title "2nd percentile", \
    "data.txt" using 1:29 with lines lc "grey50" title "1st percentile", \
    "data.txt" using 1:30 with lines lc "black" title "0th percentile"
set term qt
