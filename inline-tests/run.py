#!/usr/bin/env python

import os
import popen2
import sys
import StringIO
import glob
import pprint
import shutil

pp = pprint.PrettyPrinter(indent=0)

VERBOSE = False

class Test:

    def __init__(self):
        pass

    def set_verbosity(self, flag):
       global VERBOSE
       VERBOSE = flag
       return


    def is_verbose(self):
       global VERBOSE
       return VERBOSE


    def report_error(self, somestuff):
       if len(somestuff) > 0:
          print >> sys.stderr, somestuff
          sys.stderr.flush()
       return


    def report_info(self, somestuff):
       global VERBOSE
       if VERBOSE and len(somestuff) > 0:
          print >> sys.stdout, somestuff
          sys.stdout.flush()
       return


    def run_cmd_capture(self, cmd):
       (cout, cin, cerr) = popen2.popen3(cmd)

       cin.close()

       output = cout.readlines()
       cout.close()

       errors = cerr.readlines()
       cerr.close()
       
       return (''.join(output), ''.join(errors))


    def run_cmd(self, cmd, desc):
       global VERBOSE

       result = True

       if VERBOSE and len(desc) > 0:
          report_info('=> ' + desc)

       (cout, cin, cerr) = popen2.popen3(cmd)

       cin.close()

       output = cout.readlines()
       cout.close()
       if VERBOSE:
          report_info(''.join(output))

       errors = cerr.readlines()
       cerr.close()
       if len(errors) > 0:
          result = False
          report_error(''.join(errors))

       return result


    def sanityze(self, line):
        ret = line.split(',')
        if not ret or len(ret) < 2:
            return None

        ret[0] = ret[0].rstrip()
        ret[1] = ret[1].rstrip()

        # remove :userspace - cache-references:u
        d = ret[1].split(':')
        if d and len(d) > 1:
            ret[1] = d[0]

        if "not counted" in ret[0]:
            ret[0] = 0

        return (ret[1], ret[0])


    def clean_data_dirs(self):
        dirs = glob.glob('functions-*')
        for dirname in dirs:
            sys.stderr.write("clean directory: %s\n" % (dirname))
            shutil.rmtree(dirname)


    def recalc_results(self, results):
        if results.has_key("LLC-load-misses") and results.has_key("LLC-loads"):
            if results["LLC-load-misses"] == 0 or results["LLC-loads"] == 0:
                results["LLC-load-miss-ratio"] = 0
            else:
                results["LLC-load-miss-ratio"] = \
                        str(float(results["LLC-load-misses"]) / float(results["LLC-loads"]) * 100.0)


    def compile_run_perf(self, conf):

        events = [
                "cycles:u",
                "instructions:u",
                "cache-references:u",
                "cache-misses:u",
                "LLC-loads:u",
                "L1-dcache-loads:u",
                "L1-dcache-load-misses:u",
                "LLC-loads:u",
                "LLC-load-misses:u",
                "branch-loads:u",
                "branch-load-misses:u",
                "iTLB-loads:u",
                "iTLB-load-misses:u",
                "LLC-prefetches:u",
                "LLC-prefetch-misses:u",
                "L1-icache-loads:u",
                "L1-icache-load-misses:u",
                "branch-instructions:u",
                "branch-misses:u"
                ]

        cmd = "./generator.py --functions %d --subfunctions %d --loops %d --instruction-size %d" % \
                (conf["functions"], conf["sub-functions"], conf["instruction-size"], conf["loops"])

        cmd += " | gcc -o %s -xc -" % (self.executable_name)

        self.run_cmd(cmd, "a.out generator")

        cmd = ""
        if self.args.realtime:
            cmd += "taskset -c 1 chrt --fifo 50 "

        cmd += "perf stat --repeat %d -d -e " % (conf["repeat"])
        cmd += ",".join(events)
        cmd +=  " -x , -o /dev/stdout ./a.out"

        ret = self.run_cmd_capture(cmd)
        
        resultset = dict()
        s = StringIO.StringIO(ret[0])
        for line in s:
            ret = self.sanityze(line)
            if not ret:
                continue

            resultset[ret[0]] = ret[1]

        # finally calc some shortcuts
        self.recalc_results(resultset)

        return resultset

    def construct_path(self, conf):
        return "functions-%05d/subfunctions-%05d/instructionsize-%05d" % \
                (conf["functions"], conf["sub-functions"], conf["instruction-size"])


    def format_conf(self, conf):
        return "function no: %d, sub-function no: %d, instruction size: %d" % \
                (conf["functions"], conf["sub-functions"], conf["instruction-size"])


    def execute_test(self, conf, test_path):

        sys.stderr.write("execute [%s]\n" % (self.format_conf(conf)))

        result = self.compile_run_perf(conf)
        if not result:
            return None

        os.makedirs(test_path)

        for key in result.iterkeys():
            #sys.stdout.write("\t%s -> %s\n" % (key, result[key]))
            fd = open("%s/%s.data" % (test_path, key), "w")
            fd.write("%s\n" % (result[key]))
            fd.close()


    def save_aout(self, path):
        if not os.path.isfile(self.executable_name):
            sys.stderr.write("executable not present: %s\n" % \
                    (self.executable_name))
            return
        shutil.move(self.executable_name, path)


    def cmd_parser(self):

        parser = argparse.ArgumentParser()
        parser.add_argument('--realtime', action="store_true", default=False, dest='realtime')
        self.args = parser.parse_args()



    def do(self):

        self.clean_data_dirs()
        self.executable_name = "a.out"

        conf = dict()

        conf["loops"]  = 5000
        conf["repeat"] = 5 # for perf stat

        conf["functions"] = 100
        while conf["functions"] < 120:

            # sub-function number
            conf["sub-functions"] = 40
            while conf["sub-functions"] < 200:
                
                # instruction size
                conf["instruction-size"] = 40
                while conf["instruction-size"] < 350:

                    path = self.construct_path(conf)
                    self.execute_test(conf, path)
                    self.save_aout(path)

                    conf["instruction-size"] += 35
                conf["sub-functions"] += 10
            conf["functions"] += 40

if __name__ == "__main__":
    Test().do()
