# DMC (diagram model counter)
DMC solves ER-SSAT given a graded project-join tree for a CNF formula

--------------------------------------------------------------------------------

## Installation (Linux)

### Prerequisites
#### External libraries
- automake 1.16
- cmake 3.16
- g++ 9.3
- gmp 6.2
- make 4.2
#### [Included libraries](../addmc/libraries/)
- [cudd 3.0](https://github.com/ivmai/cudd)
- [cxxopts 2.2](https://github.com/jarro2783/cxxopts)
- [sylvan 1.5](https://github.com/trolando/sylvan)

### Command
```bash
make dmc
```

--------------------------------------------------------------------------------

## Examples

### Showing command-line options
#### Command
```bash
./dmc
```
#### Output
```
Diagram Model Counter (reads join tree from stdin)
Usage:
  dmc [OPTION...]

      --cf arg  cnf file path; string (REQUIRED)
      --wc arg  weighted counting: 0, 1; int (default: 0)
      --pc arg  projected counting: 0, 1; int (default: 0)
      --er arg  exist-random stochastic satisfiability: 0, 1; int (default: 0)
      --ma arg  maximizing assignment [with er_arg = 1]: 0, 1; int (default: 0)
      --pw arg  planner wait duration (in seconds); float (default: 0.200000)
      --dp arg  diagram package: c/CUDD, s/SYLVAN; string (default: c)
      --tc arg  thread count, or 0 for hardware_concurrency value; int (default: 1)
      --ts arg  thread slice count [with dp_arg = c]; int (default: 1)
      --rs arg  random seed; int (default: 0)
      --dv arg  diagram var order: 0/RANDOM, 1/DECLARED, 2/MOST_CLAUSES, 3/MINFILL, 4/MCS, 5/LEXP,
                6/LEXM (negative for inverse order); int (default: 4)
      --sv arg  slice var order [with dp_arg = c]: 0/RANDOM, 1/DECLARED, 2/MOST_CLAUSES, 3/MINFILL,
                4/MCS, 5/LEXP, 6/LEXM, 7/BIGGEST_NODE, 8/HIGHEST_NODE (negative for inverse order); int
                (default: 7)
      --ms arg  mem sensitivity (in MB) for reporting usage [with dp_arg = c]; float (default: 1e3)
      --mm arg  max mem (in MB) for unique table and cache table combined; float (default: 4e3)
      --tr arg  table ratio [with dp_arg = s]: log2(unique_size/cache_size); int (default: 1)
      --ir arg  init ratio for tables [with dp_arg = s]: log2(max_size/init_size); int (default: 10)
      --mp arg  multiple precision [with dp_arg = s]: 0, 1; int (default: 0)
      --lc arg  log counting [with dp_arg = c]: 0, 1; int (default: 0)
      --jp arg  join priority: a/ARBITRARY_PAIR, b/BIGGEST_PAIR, s/SMALLEST_PAIR; string (default: s)
      --vc arg  verbose cnf processing: 0, 1, 2; int (default: 0)
      --vj arg  verbose join-tree processing: 0, 1, 2; int (default: 0)
      --vp arg  verbose profiling: 0, 1, 2; int (default: 0)
      --vs arg  verbose solving: 0, 1, 2; int (default: 1)
```

### Solving ER-SSAT given CNF formula from file and join tree from planner
#### Command
```bash
cnfFile=../examples/blasted_case206.cnf && ../lg/lg.sif "/solvers/flow-cutter-pace17/flow_cutter_pace17 -p 100" <$cnfFile | ./dmc --cf=$cnfFile --wc=1 --pc=1 --er=1 --ma=1
```
#### Output
```
c processing command-line options...
c cnfFile                     ../examples/blasted_case206.cnf
c weightedCounting            1
c projectedCounting           1
c existRandom                 1
c maximizingAssignment        1
c plannerWaitSeconds          0.2
c diagramPackage              CUDD
c threadCount                 1
c threadSliceCount            1
c randomSeed                  0
c diagramVarOrder             MCS
c sliceVarOrder               BIGGEST_NODE
c memSensitivityMegabytes     1000
c maxMemMegabytes             4000
c logCounting                 0
c joinPriority                SMALLEST_PAIR

c processing cnf formula...

c procressing join tree...
c getting join tree from stdin with 0.2s timer (end input with 'enter' then 'ctrl d')
c processed join tree ending on line 16
c joinTreeWidth               7
c plannerSeconds              0.0116854
c processed join tree ending on line 32
c joinTreeWidth               7
c plannerSeconds              0.0173666
c received SIGALRM after 0.2s
c found join tree; killing planner
c killed planner process with pid 389918
c getting join tree from stdin: done

c computing output...
c diagramVarSeconds           0
c thread slice counts: { 1 }
c sliceWidth                  7
c threadMaxMemMegabytes       4000
c thread    1/1 | assignment    1/1: {  }
c thread    1/1 | assignment    1/1 | seconds 0.021      | mc 0.03456
c apparentSolution            0.03456
c ------------------------------------------------------------------
s SATISFIABLE
c s type pmc
c s log10-estimate -1.46143
c s exact double prec-sci 0.03456
c ------------------------------------------------------------------
v 11 9 8 -2 -13 -5 -14 1 -12 0
c seconds                     0.224
```
