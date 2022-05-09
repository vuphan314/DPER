#!/usr/bin/env python3

import click
import functools
import os
import subprocess
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

CUDD = 'c'
SYLVAN = 's'
DIAGRAM_PACKAGES = (CUDD, SYLVAN)

def getAbsPath(*args):
    return os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), *args))

def getBinPath(fileName):
    return getAbsPath('bin', fileName)

def cat(cmd):
    assert isinstance(cmd, list), cmd
    return ' '.join(cmd)

def sh(cmd, **kwargs):
    subprocess.run(cat(cmd), shell=True, **kwargs)

def printPair(key, value): # for postprocessor.py
    print(f'c {key} {value}')

@click.command(context_settings={'max_content_width': 90, 'help_option_names': ['-h', '--help']})
@click.option('--cf', help='cnf file', required=True)
@click.option('--solver', help='planner/executor/counter', type=click.Choice(SOLVERS), default=DPMC, show_default=True)
@click.option('--decomposer', help='planning technique', type=click.Choice(DECOMPOSERS), default=FLOW, show_default=True)
@click.option('--jf', help='jt file', default='', show_default=True)
@click.option('--sif', help='dmc.sif instead of dmc', default=0, show_default=True)
@click.option('--softmemcap', help='for solver, in GB', default=0., show_default=True)
@click.option('--memcap', help='for runsolver, in GB', default=0., show_default=True)
@click.option('--timecap', help='for runsolver, in seconds', default=0., show_default=True)
@click.option('--runner', help='runsolver', default=0, show_default=True)
@click.option('--tc', help='thread count', default=1, show_default=True)
@click.option('--wc', help='weighted counting', default=0, show_default=True)
@click.option('--pc', help='projected counting', default=0, show_default=True)
@click.option('--er', help='exist-random stochastic satisfiability', default=0, show_default=True)
@click.option('--ma', help='maximizing assignment', default=0, show_default=True)
@click.option('--pw', help='planner wait duration', default=0., show_default=True)
@click.option('--dp', help='diagram package', type=click.Choice(DIAGRAM_PACKAGES), default=CUDD, show_default=True)
@click.option('--ts', help='thread slice count', default=1, show_default=True)
@click.option('--sv', help='slice var order', default=7, show_default=True)
@click.option('--vj', help='verbose join-tree processing', default=0, show_default=True)
@click.option('--vp', help='verbose profiling', default=0, show_default=True)
@click.option('--vs', help='verbose solving', default=1, show_default=True)
@click.option('--extra', help='other arguments', default='', show_default=True)
def main(cf, solver, decomposer, jf, sif, softmemcap, memcap, timecap, runner, tc, wc, pc, er, ma, pw, dp, ts, sv, vj, vp, vs, extra):
    def getRunnerCmd():
        cmd = [getBinPath('runsolver'), '-w /dev/null', '-v /dev/stdout']
        if memcap:
            cmd.append(f'-R {int(memcap * 1e3)}') # in MB (must not use -M or -V)
        if timecap:
            cmd.append(f'-W {int(timecap)}')
        return cmd

    printPair('cf', cf)
    printPair('solver', solver)
    printPair('decomposer', decomposer)
    print()

    cmd = getRunnerCmd() if runner else []

    if solver in {PLANNER, DMC, DPMC}:
        cmd += [
            getAbsPath('dpmc.py'),
            f'--cf={cf}',
            f'--solver={solver}',
            f'--decomposer={decomposer}',
            f'--jf={jf}',
            f'--sif={sif}',
            f'--softmemcap={softmemcap}',
            f'--tc={tc}',
            f'--wc={wc}',
            f'--pc={pc}',
            f'--er={er}',
            f'--ma={ma}',
            f'--pw={pw}',
            f'--dp={dp}',
            f'--ts={ts}',
            f'--sv={sv}',
            f'--vj={vj}',
            f'--vp={vp}',
            f'--vs={vs}'
        ]
        sh(cmd, stderr=(sys.stderr if solver == PLANNER and decomposer != HTB else sys.stdout))
    elif solver == ERSSAT:
        verbosity = '-v' if vs else ''
        cmd += [
            getBinPath('abc'),
            '-c',
            f'"ssat {verbosity} {extra} {cf}"' # https://github.com/NTU-ALComLab/ssatABC/issues/15
        ]
        sh(cmd, stderr=sys.stdout)
    elif solver == DCSSAT:
        cmd += [getBinPath('dcssat'), cf]
        sh(cmd, stderr=sys.stdout)
    elif solver == ANTOM:
        cmd += [
            getBinPath('countAntom_1.0_static_linux_x64'),
            f'--memSize={int(softmemcap * 1e3)}',
            f'--noThreads={tc}',
            cf
        ]
        p = subprocess.Popen(cat(cmd), shell=True, stderr=sys.stdout)
        printPair('pid', p.pid)
    elif solver == NESTHDB:
        cmd += [
            getBinPath('nesthdb.py'),
            f'--config={getBinPath("config.json")}',
            f'--file={cf}'
        ]
        sh(cmd, stderr=sys.stdout)
    elif solver == D4P:
        cmd += [
            getBinPath('d4'),
            '-pv=NO',
            '-mc',
            cf,
            f'-fpv={cf}.var'
        ]
        if wc:
            cmd.append(f'-wFile={cf}.weight')
        sh(cmd, stderr=sys.stdout)
    elif solver == PROJMC:
        cmd += [
            getBinPath('d4'),
            '-emc',
            cf,
            f'-fpv={cf}.var'
        ]
        if wc:
            cmd.append(f'-wFile={cf}.weight')
        sh(cmd, stderr=sys.stdout)
    else:
        assert solver == MINISAT, solver
        cmd += [
            getBinPath('cryptominisat5_amd64_linux_static'),
            f'--verb={vs}',
            cf
        ]
        sh(cmd, stderr=sys.stdout)

if __name__ == '__main__':
    main()
