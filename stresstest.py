# -*- coding: utf8 -*-

import os
import signal
from subprocess import Popen, PIPE, TimeoutExpired
from timeit import default_timer as timer
from statistics import mean

#
# CONFIGURATION begin
#

TIMEOUT = 60 * 60 * 6  # in seconds
REPETITIONS = 2

EXECUTABLE = "./build/bin/plankton"
BENCHMARKS = [
    "examples/buggy/01_null_VechevYahavDCas.pl",
    "examples/buggy/02_nolink_Harris.pl",
    "examples/buggy/02_nolink_Michael.pl",
    "examples/buggy/02_nolink_ORVYY.pl",
    "examples/buggy/02_nolink_VechevYahavCas.pl",
    "examples/buggy/02_nolink_VechevYahavDCas.pl",
    "examples/buggy/03_cycle_Harris.pl",
    "examples/buggy/03_cycle_Michael.pl",
    "examples/buggy/03_cycle_ORVYY.pl",
    "examples/buggy/03_cycle_VechevYahavCas.pl",
    "examples/buggy/03_cycle_VechevYahavDCas.pl",
    "examples/buggy/04_linkmarked_Michael.pl",
    "examples/buggy/04_linkmarked_ORVYY.pl",
    "examples/buggy/04_linkmarked_VechevYahavCas.pl",
    "examples/buggy/04_linkmarked_VechevYahavDCas.pl",
    "examples/buggy/05_wrongkey_Harris.pl",
    "examples/buggy/05_wrongkey_Michael.pl",
    "examples/buggy/05_wrongkey_ORVYY.pl",
    "examples/buggy/05_wrongkey_VechevYahavCas.pl",
    "examples/buggy/06_ignoremark_Harris.pl",
    "examples/buggy/06_ignoremark_Michael.pl",
    "examples/buggy/06_ignoremark_ORVYY.pl",
    "examples/buggy/06_ignoremark_VechevYahavCas.pl",
    "examples/buggy/07_duplicate_Harris.pl",
    "examples/buggy/07_duplicate_Michael.pl",
    "examples/buggy/07_duplicate_ORVYY.pl",
    "examples/buggy/07_duplicate_VechevYahavCas.pl",
    "examples/buggy/07_duplicate_VechevYahavDCas.pl",
    "examples/buggy/08_mark_Harris.pl",
    "examples/buggy/08_mark_Michael.pl",
    "examples/buggy/08_mark_ORVYY.pl",
    "examples/buggy/08_mark_VechevYahavCas.pl",
    "examples/buggy/09_nomark_Harris.pl",
    "examples/buggy/09_nomark_Michael.pl",
    "examples/buggy/09_nomark_ORVYY.pl",
    "examples/buggy/09_nomark_VechevYahavCas.pl"
]

#
# CONFIGURATION end
#


RESULTS = {}


def average(array, places=0):
    return round(mean(array), places)


def human_readable(ms):
    m, s = divmod(round(ms/1000, 0), 60)
    return "{}m{:0>2}s".format(int(m), int(s))


def extract_info(code, time):
    if code == 0:
        return "unsound ✗"
    else:
        return human_readable(time) + " ✓"


def run_with_timeout(path):
    all_args = [EXECUTABLE, "-g", path]

    # make sure to properly kill subprocesses after timeout
    # see: https://stackoverflow.com/questions/36952245/subprocess-timeout-failure
    with Popen(all_args, stderr=PIPE, stdout=PIPE, preexec_fn=os.setsid, universal_newlines=True) as process:
        try:
            start = timer()
            output = process.communicate(timeout=TIMEOUT)[0]
            end = timer()
            return output, process.returncode, end-start
        except TimeoutExpired:
            os.killpg(process.pid, signal.SIGINT)
            return "__to__", -1, -1


def run_test(path, i):
    output, code, time = run_with_timeout(path)
    time = int(round(time * 1000, 0))
    result = extract_info(code, time)
    print("[{:0>2}/{:0>2}] {:>18}  for  {:<55}".format(i+1, REPETITIONS, result, path), flush=True)
    return code, time


def finalize():
    print()
    print()
    header = "{:<55} | {:>15}"
    print(header.format("Program", "Bug detected"))
    print("---------------------------------------------------------+-----------------")
    for path in BENCHMARKS:
        if 0 in [code for (code, time) in RESULTS.get(path, [])]:
            print(header.format(path, "unsound ✗"))
        else:
            times = [time for (code, time) in RESULTS.get(path, [])]
            avg = human_readable(average(times)) + " ✓"
            print(header.format(path, avg))


def main():
    print("Settings: iterations={0}, timeout={1}".format(REPETITIONS, human_readable(TIMEOUT*1000)))
    print("Running stress tests...")
    print()
    for i in range(REPETITIONS):
        for path in BENCHMARKS:
            code, time = run_test(path, i)
            RESULTS[path] = RESULTS.get(path, []) + [(code, time)]
    finalize()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
        finalize()
