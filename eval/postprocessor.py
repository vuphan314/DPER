#!/usr/bin/env python3

import click
import functools
import os
import signal
import sys

cerr = print = functools.partial(print, flush=True)

PLANNER = 'planner'
DMC = 'dmc'
DPMC = 'dpmc'
DPMC_SOLVERS = (PLANNER, DMC, DPMC)

ERSSAT = 'erssat'
DCSSAT = 'dcssat'
ANTOM = 'antom' # countAntom
NESTHDB = 'nesthdb'
D4P = 'd4p'
PROJMC = 'projmc'
MINISAT = 'minisat' # CryptoMiniSat
SOLVERS = DPMC_SOLVERS + (ERSSAT, DCSSAT, ANTOM, NESTHDB, D4P, PROJMC, MINISAT)

HTB = 'htb'
HTD = 'htd'
FLOW = 'flow'
DECOMPOSERS = (HTB, HTD, FLOW)

treeWidths = []
treeTimes = []

def signalPids(pids, sig=signal.SIGTERM):
    for i in range(len(pids)):
        pid = pids[i]
        # printOut(f'pid{i + 1}', pid)
        try:
            os.kill(pid, sig)
        except ProcessLookupError:
            pass

def printOut(key, value): # for .out file
    print(f'{key}:{value}')

def printBase(benchmarkPath):
    printOut('base', os.path.splitext(os.path.basename(benchmarkPath))[0])

def printSol(s):
    printOut('sol', s)

def printTime(t): # runner
    printOut('time', t)

def printMem(m): # runner, in GB
    printOut('mem', m)

def printRunnerLine(line, solver):
    (k, v) = line.split('=')
    if k == 'WCTIME':
        time = float(v)
        if solver == DMC:
            printOut('exetime', time)
            if treeTimes:
                time += treeTimes[-1]
        printTime(time)
    elif k == 'MAXMM': # in KB; determines TIMEOUT (must not use MAXVM or MAXRSS)
        printMem(float(v) / 1e6)
    elif k == 'TIMEOUT':
        printOut('timeout', int(v == 'true'))
    elif k == 'MEMOUT':
        printOut('memout', int(v == 'true'))
    elif k == 'EXITSTATUS':
        printOut('exit', v)

def printTreeTimeLine(line):
    t = float(line.split()[-1])
    treeTimes.append(t)
    printOut(f'treetime{len(treeWidths)}', t)

def printDpmcLine(line, solver, decomposer, pids):
    runningLg = solver == PLANNER and decomposer != HTB

    words = line.split()

    if line.startswith('c joinTreeWidth'): # planner or DMC
        width = int(words[-1])
        treeWidths.append(width)
        printOut(f'width{len(treeWidths)}', width)
    elif line.startswith('c seconds') and solver == PLANNER:
        printTreeTimeLine(line)
        # signalPids(pids, signal.SIGKILL) # SIGTERM would result in duplicated tree

    elif line.startswith('c plannerSeconds'): # DMC
        printTreeTimeLine(line)
    elif line.startswith('c diagramVarSeconds'):
        printOut('dvtime', words[-1])
    elif line.startswith('c sliceVarSeconds'):
        printOut('svtime', words[-1])
    elif line.startswith('c sliceAssignmentsSeconds'):
        printOut('satime', words[-1])
    elif line.startswith('c sliceWidth'):
        printOut('swidth', words[-1])
    elif line.startswith('c s log10-estimate'):
        printOut('log10', words[-1])
    elif line.startswith('c s exact'):
        printSol(words[-1])

def printTrees():
    if treeWidths:
        printOut('treecount', len(treeTimes))
        printOut('width', treeWidths[-1])
        printOut('treetime', treeTimes[-1])

@click.command(context_settings={'max_content_width': 90, 'help_option_names': ['-h', '--help']})
@click.option('--verbose', help='copies stdin to stderr', default=1, show_default=True)
def main(verbose):
    solver = '' # from wrapper.py
    decomposer = '' # from wrapper.py
    pids = [] # from wrapper.py or LG
    solverEnded = False

    for line in sys.stdin:
        line = line.rstrip()
        words = line.split()

        if line.startswith('#'):
            if not solverEnded:
                solverEnded = True
                printTrees()
        elif solverEnded:
            printRunnerLine(line, solver)

        elif line.startswith('c cf'): # wrapper.py
            printBase(words[-1])
        elif line.startswith('c solver'): # wrapper.py
            solver = words[-1]
        elif line.startswith('c decomposer'): # wrapper.py
            decomposer = words[-1]
        elif line.startswith('c pid'): # wrapper.py or LG
            pids.append(int(words[-1]))

        elif solver in DPMC_SOLVERS:
            printDpmcLine(line, solver, decomposer, pids)

        elif solver == 'erssat':
            if line.startswith('  > Satisfying probability:'):
                printSol(words[-1])
        elif solver == 'dcssat':
            if line.startswith('Pr[SAT]'):
                printSol(words[-1])
        elif solver == 'antom':
            if line.startswith('c model count'):
                printSol(line.split(': ')[1])
            elif line.startswith('s '):
                assert len(pids) == 1, pids
                signalPids([ # countAntom would hang after solving otherwise
                    pids[0] # pid of sh
                    + 1 # pid of runsolver
                ])
        elif solver == NESTHDB:
            if line.startswith('PMC:'):
                printSol(words[-1])
        elif solver in {D4P, PROJMC}:
            if line.startswith('s '):
                printSol(words[1])
        elif solver == MINISAT:
            if line.startswith('s '):
                printSol(int(words[1] == 'SATISFIABLE'))

        if verbose:
            print(line, file=sys.stderr)

    if not solverEnded: # in case runsolver fails
        printTrees()

if __name__ == '__main__':
    main()
