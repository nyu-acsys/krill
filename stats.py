# -*- coding: utf8 -*-
import math


# Configuration
DATABASE = "build/eval_M1.txt"


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
    size_map = {}
    total_map = {}
    full_map = {}
    percentage = 0
    print("Processing '{0}'...".format(DATABASE), end='', flush=True)
    with open(DATABASE) as file:
        num_lines = sum([1 for _ in file])
        file.seek(0)
        for number, line in enumerate(file):
            method, size, time, benchmark, rep, _ = line.rstrip().split(',', 5)
            size = int(size)
            time = int(time)
            rep = int(rep)
            #
            size_key = (method, size)
            size_map[size_key] = size_map.get(size_key, []) + [time]
            #
            total_key = (method, benchmark)
            total_map[total_key] = total_map.get(total_key, {})
            total_map[total_key][rep] = total_map.get(total_key, {}).get(rep, 0) + time
            #
            full_map[method] = full_map.get(method, {})
            full_map[method][rep] = full_map[method].get(rep, 0) + time
            #
            new_percentage = int(((number+1) / num_lines) * 100)
            if percentage != new_percentage:
                percentage = new_percentage
                if percentage%10 == 0:
                    print(" {0}%".format(percentage), end='', flush=True)
    benchmark_map = { key: [x for x in value.values()] for key, value in total_map.items() }
    method_map = { key: [x for x in value.values()] for key, value in full_map.items() }
    print(" done!")

    print()
    print("  #Footprint    |                Method |     Average |    Deviation ")
    print(" ---------------+-----------------------+-------------+--------------")
    for (method, size), times in size_map.items():
        print("  {:<13} | {:<20} | {:>12} | {:>12}".format(size, method, micros(average(times)), micros(deviation(times))))

    print()
    print("  Benchmark     |               Method |      Average |    Deviation ")
    print(" ---------------+----------------------+--------------+--------------")
    for (method, benchmark), times in benchmark_map.items():
        print("  {:<13} | {:<20} | {:>12} | {:>12}".format(benchmark, method, micros(average(times)), micros(deviation(times))))

    print()
    print("  Method               |      Average |    Deviation ")
    print(" ----------------------+--------------+--------------")
    for method, times in method_map.items():
        print("  {:<20} | {:>12} | {:>12}".format(method, micros(average(times)), micros(deviation(times))))

    # print()
    # print("  Method               |    #Footprint |      Average |    Deviation ")
    # print(" ----------------------+---------------+--------------+--------------")
    # for (method, size), times in size_map.items():
    #     print("  {:<20} | {:>13} | {:>12} | {:>12}".format(method, size, micros(average(times)), micros(deviation(times))))

    # print()
    # print("  Method               |     Benchmark |      Average |    Deviation")
    # print(" ----------------------+---------------+--------------+--------------")
    # for (method, benchmark), times in benchmark_map.items():
    #     print("  {:<20} | {:>13} | {:>12} | {:>12}".format(method, benchmark, micros(average(times)), micros(deviation(times))))

    # print()
    # print("  Method               |      Average |    Deviation")
    # print(" ----------------------+--------------+--------------")
    # for method, times in method_map.items():
    #     print("  {:<20} | {:>12} | {:>12}".format(method, micros(average(times)), micros(deviation(times))))


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
