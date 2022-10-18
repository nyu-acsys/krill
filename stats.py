# -*- coding: utf8 -*-
import math


# Configuration
DATABASE = "build-mac/foo.txt"


def average(list):
    if not list: return 0
    return int(sum(list) / len(list))


def deviation(list):
    if not list: return 0
    avg = average(list)
    diffs = [(x - avg)**2 for x in list]
    return int(math.sqrt(sum(diffs) / len(list)))


def extract_count(counts):
    counts = set(counts)
    if not counts or len(counts) > 1:
        return None
    return list(counts)[0]


def print_latex_prolog():
    print("""
\\documentclass[border=5pt]{standalone}
\\usepackage{pgfplots}
\\usepackage{tabularx}
\\usepackage{booktabs}
\\newcommand{\\timeCell}[2]{$#1\\mkern+2mu\\textrm{ms}$}
\\newcommand{\\timeCellNone}{---}
\\newcommand{\\mksec}[1]{\\section*{#1}}
\\begin{document}
\\begin{minipage}{13cm}
\\centering""")


def print_latex_epilog():
    print("""\\end{minipage}
\\end{document}""")


def make_latex_structure_cell(times):
    if not times:
        return "\\timeCellNone"
    else:
        avg = (average(times) / 1000) / 1000
        dev = deviation(times) / 1000
        return "\\timeCell{{ {0:.0f} }}{{ {1:.2f} }}".format(avg, dev)


def print_latex_structure_table(structures, methods, total_map, full_map, fp_map):
    print("\\begin{{tabularx}}{{\\textwidth}}{{ X r *{{ {0} }}{{ r }} }}".format(len(methods)))
    print("    Structures & \\#FG & ", " & ".join(methods), " \\\\", sep='')
    print("    \\midrule")
    fg_sum = 0
    for structure in structures:
        print("    ", structure, sep='', end='')
        if structure in fp_map.keys():
            count = fp_map[structure]
            fg_sum += count
        else:
            count = "---"
        print(" & ", count, sep='', end='')
        for method in methods:
            times = total_map.get((method, structure), {}).values()
            print(" & ", make_latex_structure_cell(times), sep='', end='')
        print(" \\\\")
    print("    \\midrule")
    print("    Total & ", fg_sum, sep='', end='')
    for method in methods:
        times = full_map.get(method, {}).values()
        print(" & ", make_latex_structure_cell(times), sep='', end='')
    print(" \\\\")
    print("    \\bottomrule")
    print("\\end{tabularx}")


def make_latex_footprint_plots(methods, sizes, size_map):
    for method in methods:
        coordinates = []
        for index, size in enumerate(sizes):
            times = size_map.get(method, {}).get(size, [])
            avg = average(times) / 1000000.0
            dev = deviation(times) / 1000000.0
            coordinates.append("            ({0}, {1}) +- (0.0, {2})".format(index+1, avg, dev))
        yield "        \\addplot+[,error bars/.cd,y dir=both,y explicit] coordinates {\n" + "\n".join(coordinates) + "\n        };"


def print_latex_footprint_chart(methods, sizes, size_map, label=None, legend=True, resize=False):
    print("\\begin{tikzpicture}")
    print("    \\begin{axis}[xticklabels={", sep='', end='')
    print(",".join(map(str, sizes)), sep='', end='')
    print("}", sep='', end='')
    if label:
        print(",xlabel=", label, sep='', end='')
    if resize:
        print(",width=6.5cm, height=7cm", sep='', end='')
    print(",legend pos=outer north east,legend cell align={left},enlargelimits={abs=0.5},ybar=0pt", sep='', end='')
    print(",xtick={0.5,1.5,...,100},x tick label as interval,area legend]")
    for plot in make_latex_footprint_plots(methods, sizes, size_map):
        print(plot)
    if legend:
        print("        \\legend{", ",".join(methods), "}", sep='')
    print("    \\end{axis}")
    print("\\end{tikzpicture}")


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
    graph_map = {}
    all_structures = set()
    all_methods = set()
    all_sizes = set()
    # print("File: '{0}'".format(DATABASE))
    with open(DATABASE) as file:
        for p, method, structure, rep, size, time in read_file(file):
            # some counting
            all_structures.add(structure)
            all_methods.add(method)
            all_sizes.add(size)
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
            # counting graphs
            graph_map.setdefault(structure, {})
            graph_map[structure].setdefault(method, {})
            graph_map[structure][method].setdefault(rep, 0)
            graph_map[structure][method][rep] += 1
            # print("\rProcessing: {:>3}%".format(p), end="")
    # print("\rComplete: 100%      ")
    all_structures = [x for x in sorted(all_structures)]
    all_methods = [x for x in all_methods]
    all_sizes = [x for x in all_sizes]
    graph_map = {structure: extract_count(sum([list(x.values()) for x in mmap.values()], [])) for structure, mmap in graph_map.items()}
    graph_map = {structure: count for structure, count in graph_map.items() if count}

    # latex
    # python3 stats.py | pdflatex -jobname=evaluation
    print_latex_prolog()
    print("%%\n%% runtime table\n%%")
    print("\\mksec{Runtimes}")
    print_latex_structure_table(all_structures, all_methods, total_map, full_map, graph_map)
    print("%%\n%% footprint chart\n%%")
    print("\\mksec{Footprints}")
    print_latex_footprint_chart(all_methods, all_sizes, size_map)
    print("%%\n%% structure charts\n%%")
    print("\\mksec{Detailed Footprints}")
    for i, structure in enumerate(all_structures):
        smap = struct_size_map.get(structure, None)
        if not map: continue
        print("%% chart for", structure)
        print_latex_footprint_chart(all_methods, all_sizes, smap, structure, False, True)
        if i + 1 < len(all_structures):
            print("~~")
    print("%%\n%% end\n%%")
    print_latex_epilog()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("", flush=True)
        print("", flush=True)
        print("[interrupted]", flush=True)
