# -*- coding: utf8 -*-

import os
import signal
from subprocess import Popen, PIPE, TimeoutExpired
import re

TIMEOUT = 60 * 15  # in seconds
REPETITIONS = 5

EXECUTABLE = "./cmake-build-debug/bin/plankton"
BENCHMARKS = [
    "examples/VechevYahavCas.pl",
    "examples/VechevYahavDCas.pl",
    "examples/ORVYY1.pl",
    "examples/ORVYY2.pl",
    "examples/Michael.pl",
]

REGEX = r"@gist\[(?P<path>.*?)\]=(?P<result>[01]),(?P<time>[0-9]*);(.*)"

RESULT = {}


def extract_info(output):
    if output == "__to__":
        return False, "timeout"
    if output == "__fail__":
        return False, "error"

    m = re.search(REGEX, output)
    if not m:
        return False, "error"
    if m.group("result") != "1":
        return False, "fail"
    return True, m.group("time")


def run_with_timeout(name):
    all_args = [EXECUTABLE, "-g", name]

    # make sure to properly kill subprocesses after timeout
    # see: https://stackoverflow.com/questions/36952245/subprocess-timeout-failure
    with Popen(all_args, stderr=PIPE, stdout=PIPE, preexec_fn=os.setsid, universal_newlines=True) as process:
        try:
            output = process.communicate(timeout=TIMEOUT)[0]
            if process.returncode != 0:
                output = "__fail__"
        except TimeoutExpired:
            os.killpg(process.pid, signal.SIGINT)
            output = "__to__"
    return output


def run_test(path, i):
    output = run_with_timeout(path)
    success, info = extract_info(output)
    time = None
    if success:
        time = int(info)
        info += "ms"
    print("[{:0>2}/{:0>2}] {:>15}  for  {:<20}".format(i+1, REPETITIONS, info, path), flush=True)
    return time


def finalize():
    print()
    for path in BENCHMARKS:
        times = [x for x in RESULT.get(path, []) if not None]
        avg = int(sum(times) / len(times)) if len(times) > 0 else -1
        avg = str(avg) + "ms" if avg >= 0 else "--"
        print("==avg== {:>15}  for  {:<20}".format(avg, path), flush=True)


def main():
    for i in range(REPETITIONS):
        for path in BENCHMARKS:
            time = run_test(path, i)
            RESULT[path] = RESULT.get(path, []) + [time]
    finalize()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
        finalize()
