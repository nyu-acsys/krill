# -*- coding: utf8 -*-

import sys
import os
import signal
from subprocess import Popen, PIPE, TimeoutExpired
from time import monotonic as timer


TIMEOUT = 60*15 # in seconds
REPETITIONS = 5
SEPARATOR = "!@#$%^&*()"
BINPREFIX = "cmake-build-debug/bin/benchmark_"


def extract_time(output):
    if output == "to": return -2
    if output == "fail": return -1
    gist = output.split(SEPARATOR)[-1]
    gist = gist.split("## Time taken: ")[1]
    gist = gist.split("ms")[0]
    return gist

def run_with_timeout(name):
    path_program = "./" + BINPREFIX + name
    all_args = [path_program]

    # make sure to properly kill subprocesses after timeout
    # see: https://stackoverflow.com/questions/36952245/subprocess-timeout-failure
    start = timer()
    with Popen(all_args, stderr=PIPE, stdout=PIPE, preexec_fn=os.setsid, universal_newlines=True) as process:
        try:
            output = process.communicate(timeout=TIMEOUT)[0]
            if process.returncode != 0: output = "fail"
        except TimeoutExpired:
            os.killpg(process.pid, signal.SIGINT) # send signal to the process group
            output = "to"
    return output

def run_test(name):
    for i in range(REPETITIONS):
        output = run_with_timeout(name)
        time = extract_time(output)
        if time != -1 and time != -2: time + "ms"
        print("[{:>1}/{:>1}] {:<15}: {:>15}".format(i, REPETITIONS, name, time), flush=True)

def main():
    run_test("VechevYahavDCAS")
    run_test("ORVYY")
    run_test("ORVYYmod")
    run_test("VechevYahavCAS")
    run_test("Michael")


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
