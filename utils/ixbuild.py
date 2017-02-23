#!/usr/bin/env python

from __future__ import print_function

import os
import subprocess


def default_cmake_options():
    options = {}
    options["CMAKE_EXPORT_COMPILE_COMMANDS:BOOL"] = "ON"
    options["CLANG_ENABLE_ARCMT:BOOL"] = "OFF"
    options["CLANG_ENABLE_STATIC_ANALYZER:BOOL"] = "OFF"
    options["CLANG_INCLUDE_TESTS:BOOL"] = "OFF"
    options["LLVM_INCLUDE_TESTS:BOOL"] = "OFF"
    options["HLSL_INCLUDE_TESTS:BOOL"] = "ON"
    options["LLVM_TARGETS_TO_BUILD:STRING"] = "None"
    options["LLVM_INCLUDE_DOCS:BOOL"] = "OFF"
    options["LLVM_INCLUDE_EXAMPLES:BOOL"] = "OFF"
    options["LIBCLANG_BUILD_STATIC:BOOL"] = "ON"
    options["LLVM_OPTIMIZED_TABLEGEN:BOOL"] = "OFF"
    options["LLVM_REQUIRES_EH:BOOL"] = "ON"
    options["LLVM_APPEND_VC_REV:BOOL"] = "ON"
    options["LLVM_ENABLE_RTTI:BOOL"] = "ON"
    options["LLVM_ENABLE_EH:BOOL"] = "ON"
    options["LLVM_DEFAULT_TARGET_TRIPLE:STRING"] = "dxil-ms-dx"
    options["CLANG_BUILD_EXAMPLES:BOOL"] = "OFF"
    options["LLVM_REQUIRES_RTTI:BOOL"] = "ON"
    options["CLANG_CL:BOOL"] = "OFF"
    return options


def execute(cmd):
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, universal_newlines=True)
    for line in iter(p.stdout.readline, ""):
        yield line
    p.stdout.close()
    p.wait()


def cmake_config(args):
    if not os.path.exists(args.src_dir):
        print("error: invalid source directory specified")
        return False

    if not os.path.exists(args.build_dir):
        os.makedirs(args.build_dir)

    cmake_cmd = ['cmake']

    cmake_cmd.append('-H{}'.format(args.src_dir))
    cmake_cmd.append('-B{}'.format(args.build_dir))

    if args.generator is not None:
        gen = args.generator
        if ' ' in gen:
            gen = '"{}"'.format(gen)
        cmake_cmd.append('-G{}'.format(gen))

    if args.cc is not None:
        cmake_cmd.append('-DCMAKE_C_COMPILER={}'.format(args.cc))
        if args.cxx is None:
            cmake_cmd.append('-DCMAKE_CXX_COMPILER={}++'.format(args.cc))
        else:
            cmake_cmd.append('-DCMAKE_CXX_COMPILER={}'.format(args.cxx))

    options = default_cmake_options()
    options['CMAKE_BUILD_TYPE:STRING'] = args.profile
    cmake_cmd.extend(['-D{}={}'.format(k, v) for (k, v) in options.iteritems()])

    print(' '.join(cmake_cmd))

    for line in execute(cmake_cmd):
        print(line, end="")

    return True


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Configure to generate SPIR-V')
    parser.add_argument('--profile', choices=['Debug', 'Release'],
                        default='Debug')
    parser.add_argument('-G', '--generator', type=str)
    parser.add_argument('--cc', type=str)
    parser.add_argument('--cxx', type=str)
    parser.add_argument('src_dir', metavar='<src-dir>', type=str)
    parser.add_argument('build_dir', metavar='<build-dir>', type=str)

    args = parser.parse_args()

    if args.cxx is not None and args.cc is None:
        print("error: --cxx specified but not --cc")
        exit(1)

    if not cmake_config(args):
        exit(1)

if __name__ == "__main__":
    main()
