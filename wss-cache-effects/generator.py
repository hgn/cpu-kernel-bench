#!/usr/bin/env python


import subprocess

modes = [ "random" ]

start  = [i for i in range(1000, 100000, 2000)]
middle = [i for i in range(200000, 1000000, 100000)]
end    = [i for i in range(1000000, 16000000, 1000000)]

working_sizes = start + middle + end

runs = 2

def iterations(working_size):
    if working_size < 10:
        return 100000000

    if working_size < 100:
        return 10000000

    if working_size < 1000:
        return 1000000

    if working_size < 10000:
        return 100000

    if working_size < 100000:
        return 10000

    if working_size < 1000000:
        return 1000

    if working_size < 10000000:
        return 100

    return 100

tmp = 1
for mode in modes:
    for working_size in working_sizes:
        for i in range(runs):
            tmp += 1

print("run now %d times" % (tmp))

itera = 1
for mode in modes:
    f = open(mode + ".data", 'w')
    for working_size in working_sizes:
        cmd = "./wss-cache-effects --mode %s --iteration %d --access 10000 --working-size %d" % (mode, iterations(working_size), working_size)
        res = []
        for r in range(runs):
            print("%s" % (cmd))
            p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            res.append(float(p.stdout.read().split()[0]))
            print("%lfns   [%d of %d]" % (res[-1], itera, tmp))
            itera += 1
        cum = 0.0
        for i in res:
            cum += i
        cum = cum / runs

        f.write("%d %lf\n" % (working_size, i))

    f.close()
