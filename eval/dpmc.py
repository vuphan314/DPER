#!/usr/bin/env python3

import click
import os
import subprocess

cerr = print

PLANNER = 'planner'
DMC = 'dmc'
DPMC = 'dpmc'
DPMC_SOLVERS = (PLANNER, DMC, DPMC)

HTB = 'htb'
HTD = 'htd'
FLOW = 'flow'
DECOMPOSERS = {
    HTB: '',
    HTD: '/solvers/htd-master/bin/htd_main --opt width --iterations 0 --strategy challenge --print-progress --preprocessing full',
    FLOW: '/solvers/flow-cutter-pace17/flow_cutter_pace17 -p 100'
}

CUDD = 'c'
SYLVAN = 's'
DIAGRAM_PACKAGES = (CUDD, SYLVAN)

def getBinPath(fileName):
    return os.path.join(os.path.dirname(os.path.realpath(__file__)), 'bin', fileName)

@click.command(context_settings={'max_content_width': 90, 'help_option_names': ['-h', '--help']})
@click.option('--cf', help='cnf file', required=True)
@click.option('--solver', help='planner/executor/counter', type=click.Choice(DPMC_SOLVERS), default=DPMC, show_default=True)
@click.option('--decomposer', help='planning technique', type=click.Choice(DECOMPOSERS), default=FLOW, show_default=True)
@click.option('--jf', help='jt file', default='', show_default=True)
@click.option('--sif', help='dmc.sif instead of dmc', default=0, show_default=True)
@click.option('--softmemcap', help='for solver, in GB', default=0., show_default=True)
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
def main(cf, solver, decomposer, jf, sif, softmemcap, tc, wc, pc, er, ma, pw, dp, ts, sv, vj, vp, vs):
    htbCmd = [
        getBinPath('htb'),
        f'--cf={cf}',
        f'--pc={pc}',
        f'--vs={vs}'
    ]

    lgCmd = [getBinPath('lg.sif'), DECOMPOSERS[decomposer]]

    dmcCmd = [
        'singularity',
        'run',
        '--bind="/:/host"',
        getBinPath('dmc.sif'),
        f'--cf=/host{os.path.realpath(cf)}'
    ] if sif else [getBinPath('dmc'), f'--cf={cf}'] # link=-static is safe iff tc == 1
    dmcCmd += [
        f'--wc={wc}',
        f'--pc={pc}',
        f'--er={er}',
        f'--ma={ma}',
        f'--pw={pw}',
        f'--dp={dp}',
        f'--tc={tc}',
        f'--ts={ts}',
        f'--sv={sv}',
        f'--mm={int(softmemcap * 1e3)}',
        f'--vj={vj}',
        f'--vp={vp}',
        f'--vs={vs}'
    ]
    if dp == CUDD:
        dmcCmd.append('--lc=1')

    if solver == PLANNER:
        if decomposer == HTB:
            subprocess.run(htbCmd)
        else:
            subprocess.run(lgCmd, stdin=open(cf))
    elif solver == DMC:
        assert jf, 'must specify --jf'
        subprocess.run(dmcCmd, stdin=open(jf))
    else:
        assert solver == DPMC, solver

        plannerArgs = {'stdout': subprocess.PIPE}
        if decomposer == HTB:
            cmd = htbCmd
        else:
            cmd = lgCmd
            plannerArgs['stdin'] = open(cf)
            plannerArgs['stderr'] = subprocess.DEVNULL # LG's stderr would mess up postprocessor.py

        p = subprocess.Popen(cmd, **plannerArgs)
        subprocess.run(dmcCmd, stdin=p.stdout)

if __name__ == '__main__':
    main()
