#!/usr/bin/env python
# coding=utf8

import sys
import argparse


class Generator:

    def __init__(self):
        self.fd = sys.stdout

    def create_includes(self):

        self.fd.write("#include <stdio.h>\n")
        self.fd.write("#include <stdlib.h>\n")
        self.fd.write("\n\n")

    def create_subfunctions(self):

        if self.args.inlining:
            expr = "__attribute__((always_inline))"
        else:
            expr = "__attribute__ ((noinline))"

        for j in range(self.args.subfunctions):
            self.fd.write("int %s subfunc%s(int val)\n{\n" % (expr, j))

            # 34 byte
            self.fd.write("\n\tint tmp = val * val * val * val * val;")

            i = 34
            while i < self.args.instruction: 
                i += 22 
                self.fd.write("\n\ttmp += val * val * val * val * val;")

            self.fd.write("\n\treturn tmp;\n")
            self.fd.write("}\n\n");

    def create_functions(self):

        for i in range(self.args.functions):
            self.fd.write("int func%s(int val)\n{\n" % (i))
            for j in range(self.args.subfunctions):
                self.fd.write("\tval += subfunc%i(val);\n" % (j))
            self.fd.write("\n\treturn val;\n")
            self.fd.write("}\n\n");
        

    def create_world(self):

        self.create_includes()
        self.create_subfunctions()
        self.create_functions()

        self.fd.write("int main(void)\n{\n")
        self.fd.write("\tint i, rv;\n\n");
        self.fd.write("\trv = rand();\n");

        self.fd.write("\tfor (i = 0; i < %s; i++) {\n" % (self.args.loops))

        for i in range(self.args.functions):
            self.fd.write("\t\trv += func%s(rv);\n" % (i))

        self.fd.write("\t}\n\treturn rv;\n}\n")


    def run(self):
        self.cmd_parser()
        self.create_world()

    def cmd_parser(self):
        parser = argparse.ArgumentParser()
        parser.add_argument('--loops', type=int,  default=30, dest='loops')
        parser.add_argument('--functions', type=int,  default=2, dest='functions')
        parser.add_argument('--subfunctions', type=int,  default=2, dest='subfunctions')
        parser.add_argument('--instruction-size', type=int,  default=30, dest='instruction')
        parser.add_argument('--enable-inlining', action="store_true", default=False, dest='inlining')

        # parser.print_help()
        self.args = parser.parse_args()

if __name__ == "__main__":
    Generator().run()
