# -*- coding: utf8 -*-
import math


# Configuration
DATABASE = "build-mac/foo.txt"


def average(list):
    return int(sum(list) / len(list))


def deviation(list):
    avg = average(list)
    diffs = [(x - avg)**2 for x in list]
    return int(math.sqrt(sum(diffs) / len(list)))


def make_latex_structure_cell(times):
    if not times:
        return "\\timeCellNone"
    else:
        avg = (average(times) / 1000) / 1000
        dev = deviation(times) / 1000
        return "\\timeCell{{ {0:.0f} }}{{ {1:.2f} }}".format(avg, dev)


def print_latex_structure_table(structures, methods, total_map, full_map, fp_map):
    print("\\begin{{ tabularx }}{{ X r *{{ {0} }}{{ r }} }}".format(len(methods)))
    print("    Structures & ", " & ".join(methods), " \\\\", sep='')
    print("    \\midrule")
    for structure in structures:
        print("    ", structure, sep='', end='')
        count = fp_map.get(structure, "--")
        print(" & ", count, sep='', end='')
        for method in methods:
            times = total_map.get((method, structure), {}).values()
            print(" & ", make_latex_structure_cell(times), sep='', end='')
        print(" \\\\")
    print("    \\midrule")
    print("    Total", sep='', end='')
    for method in methods:
        times = full_map.get(method, {}).values()
        print(" & ", make_latex_structure_cell(times), sep='', end='')
    print(" \\\\")
    print("    \\bottomrule")
    print("\\end{ tabularx }")


def print_latex_footprint_chart(methods, size_map):
    pass


def print_latex_footprint_structure_chart(methods, struct_size_map):
    pass


def read_file(file):
    count = sum([1 for _ in file])
    file.seek(0)
    for no, line in enumerate(file):
        method, size, time, structure, rep, _ = line.rstrip().split(',', 5)
        p = int(((no+1) / count) * 100)
        yield p, method, structure, int(rep), int(size), int(time)


def main():
    size_map = {}
    struct_size_map = {}
    total_map = {}
    full_map = {}
    all_structures = set()
    all_methods = set()
    print("File: '{0}'".format(DATABASE))
    with open(DATABASE) as file:
        for p, method, structure, rep, size, time in read_file(file):
            # some counting
            all_structures.add(structure)
            all_methods.add(method)
            # total runtime per (data structure, method)
            total_map.setdefault((method, structure), {})
            total_map[(method, structure)].setdefault(rep, 0)
            total_map[(method, structure)][rep] += time
            # runtime per method x size
            size_map.setdefault(method, {})
            size_map[method].setdefault(size, [])
            size_map[method][size].append(time)
            # runtime per structure x method x size
            struct_size_map.setdefault(structure, {})
            struct_size_map[structure].setdefault(method, {})
            struct_size_map[structure][method].setdefault(size, [])
            struct_size_map[structure][method][size].append(time)
            # total runtime per method
            full_map.setdefault(method, {})
            full_map[method].setdefault(rep, 0)
            full_map[method][rep] += time
            # footprints per structure
            print("\rProcessing: {:>3}%".format(p), end="")
    print("\rComplete: 100%      ")
    all_structures = sorted(all_structures)

    print_latex_structure_table(all_structures, all_methods, total_map, full_map, {})

    # print()
    # print("  Method               |    #Footprint |     Average |    Deviation ")
    # print(" ----------------------+---------------+-------------+--------------")
    # for method, sizes in size_map.items():
    #     for size in sorted(sizes.keys()):
    #         times = sizes[size]
    #         print("  {:<20} | {:>13} | {:>12} | {:>12}".format(method, size, micros(average(times)), micros(deviation(times))))
    #
    # print()
    # print("  Benchmark     |               Method |    #Footprint |     Average |    Deviation ")
    # print(" ---------------+----------------------+-------------+--------------")
    # for structure, methods in struct_size_map.items():
    #     for method, sizes in methods.items():
    #         for size in sorted(sizes.keys()):
    #             times = sizes[size]
    #             print("  {:<13} | {:<20} | {:>13} | {:>12} | {:>12}".format(structure, method, size, micros(average(times)), micros(deviation(times))))
    #
    # print()
    # print("  Benchmark     |               Method |      Average |    Deviation ")
    # print(" ---------------+----------------------+--------------+--------------")
    # for (method, benchmark), reps in total_map.items():
    #     times = reps.values()
    #     print("  {:<13} | {:<20} | {:>12} | {:>12}".format(benchmark, method, micros(average(times)), micros(deviation(times))))
    #
    # print()
    # print("  Method               |      Average |    Deviation ")
    # print(" ----------------------+--------------+--------------")
    # for method, reps in full_map.items():
    #     times = reps.values()
    #     print("  {:<20} | {:>12} | {:>12}".format(method, micros(average(times)), micros(deviation(times))))


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
