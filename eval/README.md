# Evaluation (Linux)

--------------------------------------------------------------------------------

## Benchmarks

### Downloading archives to this dir (`eval`)
```bash
wget https://github.com/vuphan314/DPER/releases/download/v0/benchmarks_cnf.zip

wget https://github.com/vuphan314/DPER/releases/download/v0/benchmarks_sdimacs.zip

```

### Extracting downloaded archives into the same new dir `eval/benchmarks`
```bash
unzip benchmarks_cnf.zip

unzip benchmarks_sdimacs.zip

```

### Files
- Dir `benchmarks/cnf`: ER-SSAT formula format for DPER
  - Dir [`benchmarks/cnf/wpmc/waps`](https://github.com/meelgroup/WAPS#benchmarks)
  - Dir [`benchmarks/cnf/wpmc/bird`](https://github.com/meelgroup/ApproxMC#how-to-cite)
- Dir `benchmarks/sdimacs`: ER-SSAT formula format for DC-SSAT and reSSAT

--------------------------------------------------------------------------------

## Binary ER-SSAT solvers

### Downloading archive to this dir (`eval`)
```bash
wget https://github.com/vuphan314/DPER/releases/download/v0/bin.zip
```

### Extracting downloaded archive into dir `eval/bin`
```bash
unzip bin.zip
```

### Files
- `bin/lg.sif`: DPER's [planner](../lg)
- `bin/dmc`: DPER's [executor](../dmc)
- `bin/dcssat`: DC-SSAT (private version provided by erSSAT team)
- `bin/abc`: [erSSAT](https://github.com/NTU-ALComLab/ssatABC)
- `bin/runsolver`: [tool to control and measure resource consumption](https://github.com/utpalbora/runsolver)

### DPER
#### Command
```bash
./wrapper.py --cf=../examples/blasted_case206.cnf --solver=dpmc --wc=1 --pc=1 --er=1 --ma=1 --vs=0
```
#### Output
```
c cf ../examples/blasted_case206.cnf
c solver dpmc
c decomposer flow

c processing cnf formula...

c procressing join tree...
c getting join tree from stdin with 0.2s timer (end input with 'enter' then 'ctrl d')
c received SIGALRM after 0.2s
c found no join tree yet; will wait for first join tree then kill planner
c processed join tree ending on line 16
c joinTreeWidth               7
c plannerSeconds              0.0148471
c getting join tree from stdin: done

c computing output...
c sliceWidth                  7
c threadMaxMemMegabytes       0
c ------------------------------------------------------------------
s SATISFIABLE
c s type pmc
c s log10-estimate -1.46143
c s exact double prec-sci 0.03456
c ------------------------------------------------------------------
v 11 9 8 -2 -13 -5 -14 1 -12 0
c seconds                     0.231
```
#### ER-SSAT solution
- the maximum is `0.03456`
- a maximizer is `11 9 8 -2 -13 -5 -14 1 -12` (cube of literals)

### DC-SSAT
#### Command
```bash
./wrapper.py --cf=../examples/blasted_case206.sdimacs --solver=dcssat
```
#### Output
```
c cf ../examples/blasted_case206.sdimacs
c solver dcssat
c decomposer flow

     x1    -x2     x5     x8    -x9   -x11    x12    x13   -x14    -x3     x4    -x6    -x7    x10

solve time:              = 0.000319004
rebuild/print time:      = 3.88622e-05
   total time:           = 0.000357866

Pr[SAT]                  =    0.03456
```

### erSSAT
#### Command
```bash
./wrapper.py --cf=../examples/blasted_case206.sdimacs --solver=erssat
```
#### Output
```
c cf ../examples/blasted_case206.sdimacs
c solver erssat
c decomposer flow

ABC command line: "ssat -v  ../examples/blasted_case206.sdimacs".

[INFO] Input sdimacs file: ../examples/blasted_case206.sdimacs
[INFO] Invoking erSSAT solver with the following configuration:
  > Counting engine: BDD
  > Minimal clause selection (greedy): yes
  > Clause subsumption (subsume): yes
  > Partial assignment pruning (partial): yes
[INFO] Starting analysis ...
  > Found a better solution , value = 0.034560
	1 -2 -5 8 9 11 -12 -13 -14 0
  > Time consumed =     0.00 sec
[INFO] Stopping analysis ...
  > Found an optimizing assignment to exist vars:
	1 2 5 -8 9 -11 12 -13 14 0

==== Solving results ====

  > Satisfying probability: 3.456000e-02
  > Time =     0.01 sec
```

--------------------------------------------------------------------------------

## Data

### Downloading archive to this dir (`eval`)
```bash
wget https://github.com/vuphan314/DPER/releases/download/v0/data.zip
```

### Extracting downloaded archive into dir `eval/data`
```bash
unzip data.zip
```

### Files
- Dir `data/100g/wpmc/dpmc`: DPER
  - Files `data/100g/wpmc/dpmc/cudd/flow/*.in`: commands
  - Files `data/100g/wpmc/dpmc/cudd/flow/*.log`: outputs
- Dir `data/100g/wpmc/erssat`: erSSAT
- Dir `data/100g/wpmc/dcssat`: DC-SSAT

--------------------------------------------------------------------------------

## [Jupyter notebook](dper.ipynb)
- Near the end of the notebook, there are 2 figures used in the paper
- Run all cells again to re-generate these figures from dir `data`
