# -*- coding: utf8 -*-
import math


# Configuration
DATABASE = "build-mac/eval.txt"


def average(list):
    return int(sum(list) / len(list))


def deviation(list):
    avg = average(list)
    diffs = [(x - avg)**2 for x in list]
    return int(math.sqrt(sum(diffs) / len(list)))


def micros(time):
    # return str(int(time/1000)) + "μs"
    return str(int(time/1000)/1000.0) + "ms"


def millis(time):
    return str(int(time/1000000)) + "ms"


def convert(line):
    method, size, time, structure, rep, _ = line.rstrip().split(',', 5)
    return method, structure, int(rep), int(size), int(time)

def main():
    size_map = {}
    total_map = {}
    print("Processing '{0}'...".format(DATABASE), end='', flush=True)
    with open(DATABASE) as file:
        lines = [convert(line) for line in file]
        # total runtime per (data structure, method)
        for method, structure, rep, _, time in lines:
            total_map.setdefault((method, structure), {})
            total_map[(method, structure)].setdefault(rep, 0)
            total_map[(method, structure)][rep] += time
        # runtime per method x size
        for method, _, _, size, time in lines:
            size_map.setdefault(method, {})
            size_map[method].setdefault(size, [])
            size_map[method][size].append(time)
    print(" done!")

    print()
    print("  Method               |    #Footprint |     Average |    Deviation ")
    print(" ----------------------+---------------+-------------+--------------")
    for method, sizes in size_map.items():
        for size in sorted(sizes.keys()):
            times = sizes[size]
            print("  {:<20} | {:>13} | {:>12} | {:>12}".format(method, size, micros(average(times)), micros(deviation(times))))

    print()
    print("  Benchmark     |               Method |      Average |    Deviation ")
    print(" ---------------+----------------------+--------------+--------------")
    for (method, benchmark), reps in total_map.items():
        times = reps.values()
        print("  {:<13} | {:<20} | {:>12} | {:>12}".format(benchmark, method, micros(average(times)), micros(deviation(times))))


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
