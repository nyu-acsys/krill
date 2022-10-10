# -*- coding: utf8 -*-
import math

# Configuration
DATABASE = "build/test.txt"


def average(list):
    return int(sum(list) / len(list))


def deviation(list):
    avg = average(list)
    diffs = [(x - avg)**2 for x in list]
    return int(math.sqrt(sum(diffs) / len(list)))


def micros(time):
    return str(int(time/1000)) + "μs"


def millis(time):
    return str(int(time/1000000)) + "ms"


def main():
    print("Settings: file={0}".format(DATABASE))
    size_map = {}
    total_map = {}
    with open(DATABASE) as file:
        for line in file:
            method, size, time, benchmark, rep, _ = line.rstrip().split(',', 5)
            time = int(time)
            size_key = (method, size)
            size_map[size_key] = size_map.get(size_key, []) + [time]
            total_key = (method, benchmark)
            total_map[total_key] = total_map.get(total_key, {})
            total_map[total_key][rep] = total_map.get(total_key, {}).get(rep, 0) + time
    benchmark_map = { key: [x for x in value.values()] for key, value in total_map.items() }

    print()
    print("  Method               |    #Footprint |      Average |    Deviation ")
    print(" ----------------------+---------------+--------------+--------------")
    for (method, size), times in size_map.items():
        print("  {:<20} | {:>13} | {:>12} | {:>12}".format(method, size, micros(average(times)), micros(deviation(times))))

    print()
    print("  Method               |     Benchmark |      Average |    Deviation")
    print(" ----------------------+---------------+--------------+--------------")
    for (method, benchmark), times in benchmark_map.items():
        print("  {:<20} | {:>13} | {:>12} | {:>12}".format(method, benchmark, micros(average(times)), micros(deviation(times))))


if __name__ == '__main__':
    try:
        # if len(sys.argv) > 1:
        #     REPETITIONS = int(sys.argv[1])
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
        # finalize()
