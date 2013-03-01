#!/usr/bin/env python
# coding=utf8

import sys
import argparse
import os

class Plottr:

    def __init__(self):
        self.log = sys.stderr.write
        self.out = sys.stdout.write


    def slistdir(self, path):
        return sorted([d for d in os.listdir(path) if \
                os.path.isdir(path + os.path.sep + d)])


    def flistdir(self, path):
        return sorted([d for d in os.listdir(path) if \
                os.path.isfile(path + os.path.sep + d)])


    def is_data_argument_valid(self, subdir_path):
        if not self.args.dataname:
            self.log("no data files given (--data-name)," +
                    " please specify one of them\n")
            # iterate over files in sub-dir
            for file_name in self.flistdir(subdir_path):
                self.log("data files: %s\n" % (file_name))

            sys.exit(1)


    def get_data(self, func):
        cwd = os.getcwd()

        # iterate over dirs
        for dir_name in self.slistdir(cwd):
            self.log("process dir %s\n" % (dir_name))
            dir_path = "%s%s%s" % (cwd, os.path.sep, dir_name)

            # iterate over sub-dirs
            for subdir_name in self.slistdir(dir_path):
                subdir_path = "%s%s%s" % (dir_name, os.path.sep, subdir_name)
                self.log("  process sub-dir %s\n" % (subdir_path))

                self.is_data_argument_valid(subdir_path)

                filename_path = "%s%s%s" % (subdir_path,
                        os.path.sep, self.args.dataname)
                datum = open(filename_path ,"r").read().rstrip()
                self.log("%s datum: %s\n" % (self.args.dataname, datum))

                vals = dict()

                vals["dir_name"]      = dir_name
                vals["dir_path"]      = dir_path
                vals["subdir_name"]   = subdir_name
                vals["subdir_path"]   = subdir_path
                vals["filename_path"] = filename_path
                vals["datum"]         = datum

                func(vals)

            func(None)


    def create_gnuplot_template(self, filename):
        fd = open(filename, "w")
        fd.write("set terminal postscript enhanced eps color \"Times\" 45\n")
        fd.write("set object 1 rectangle from screen 0,0 to screen 3,3 fillcolor rgb \"#ffffff\" behind\n")
        fd.write("set output \"result.eps\"\n")
        fd.write("set size 3\n")
        fd.write("set title \"Rijndael (AES) Block Cipher Padding\"\n")
        fd.write("set style line 1 linetype 1 linecolor rgb \"#ff0000\" lw 3\n")
        fd.write("set grid xtics ytics mytics\n")
        fd.write("set xlabel \"Input [Byte]\"\n")
        fd.write("set ylabel \"Output [Byte]\"\n")
        fd.write("set hidden3d\n")
        fd.write("splot \"result.data\" with lines ls 1\n")
        fd.write("!epstopdf --outfile=result.pdf result.eps\n")
        fd.write("!rm -rf result.eps\n\n")
        fd.write("# convert input.pdf output.png\n")
        fd.write("# generate animated git:\n")
        fd.write("# convert -delay 20 -loop 0 in*.gif out.gif\n")
        fd.close()


    def process_data(self, vals):

        if not vals:
            self.out("\n")
            return

        x = vals["dir_name"].split("-")[1]
        y = vals["subdir_name"].split("-")[1]
        z = vals["datum"]

        self.out("%s %s %s\n" % (x, y, z))


    def run(self):
        self.cmd_parser()
        self.get_data(self.process_data)
        self.create_gnuplot_template("result.gpi")
        return 0


    def cmd_parser(self):
        parser = argparse.ArgumentParser()
        parser.add_argument('--data-name', dest='dataname', default=None, required=False)
        self.args = parser.parse_args()

if __name__ == "__main__":
    Plottr().run()
