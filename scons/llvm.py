"""llvm

Tool-specific initialization for LLVM

"""

#
# Copyright (c) 2009 VMware, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

import os
import os.path
import re
import sys
import distutils.version

import SCons.Errors
import SCons.Util


def generate(env):
    try:
        llvm_dir = os.environ['LLVM']
    except KeyError:
        # Do nothing -- use the system headers/libs
        llvm_dir = None
    else:
        if not os.path.isdir(llvm_dir):
            raise SCons.Errors.InternalError, "Specified LLVM directory not found"

        if env['debug']:
            llvm_subdir = 'Debug'
        else:
            llvm_subdir = 'Release'

        llvm_bin_dir = os.path.join(llvm_dir, llvm_subdir, 'bin')
        if not os.path.isdir(llvm_bin_dir):
            llvm_bin_dir = os.path.join(llvm_dir, 'bin')
            if not os.path.isdir(llvm_bin_dir):
                raise SCons.Errors.InternalError, "LLVM binary directory not found"

        env.PrependENVPath('PATH', llvm_bin_dir)

    if env['platform'] == 'windows':
        # XXX: There is no llvm-config on Windows, so assume a standard layout
        if llvm_dir is None:
            return

        # Try to determine the LLVM version from llvm/Config/config.h
        llvm_config = os.path.join(llvm_dir, 'include/llvm/Config/config.h')
        if not os.path.exists(llvm_config):
            print 'scons: could not find %s' % llvm_config
            return
        llvm_version_re = re.compile(r'^#define PACKAGE_VERSION "([^"]*)"')
        llvm_version = None
        for line in open(llvm_config, 'rt'):
            mo = llvm_version_re.match(line)
            if mo:
                llvm_version = mo.group(1)
                break
        if llvm_version is None:
            print 'scons: could not determine the LLVM version from %s' % llvm_config
            return

        env.Prepend(CPPPATH = [os.path.join(llvm_dir, 'include')])
        env.AppendUnique(CPPDEFINES = [
            '__STDC_LIMIT_MACROS', 
            '__STDC_CONSTANT_MACROS',
            'HAVE_STDINT_H',
        ])
        env.Prepend(LIBPATH = [os.path.join(llvm_dir, 'lib')])
        if llvm_version >= distutils.version.LooseVersion('2.7'):
            # 2.7
            env.Prepend(LIBS = [
                'LLVMLinker', 'LLVMipo', 'LLVMInterpreter',
                'LLVMInstrumentation', 'LLVMJIT', 'LLVMExecutionEngine',
                'LLVMBitWriter', 'LLVMX86Disassembler', 'LLVMX86AsmParser',
                'LLVMMCParser', 'LLVMX86AsmPrinter', 'LLVMX86CodeGen',
                'LLVMSelectionDAG', 'LLVMX86Info', 'LLVMAsmPrinter',
                'LLVMCodeGen', 'LLVMScalarOpts', 'LLVMInstCombine',
                'LLVMTransformUtils', 'LLVMipa', 'LLVMAsmParser',
                'LLVMArchive', 'LLVMBitReader', 'LLVMAnalysis', 'LLVMTarget',
                'LLVMMC', 'LLVMCore', 'LLVMSupport', 'LLVMSystem',
            ])
        else:
            # 2.6
            env.Prepend(LIBS = [
                'LLVMX86AsmParser', 'LLVMX86AsmPrinter', 'LLVMX86CodeGen',
                'LLVMX86Info', 'LLVMLinker', 'LLVMipo', 'LLVMInterpreter',
                'LLVMInstrumentation', 'LLVMJIT', 'LLVMExecutionEngine',
                'LLVMDebugger', 'LLVMBitWriter', 'LLVMAsmParser',
                'LLVMArchive', 'LLVMBitReader', 'LLVMSelectionDAG',
                'LLVMAsmPrinter', 'LLVMCodeGen', 'LLVMScalarOpts',
                'LLVMTransformUtils', 'LLVMipa', 'LLVMAnalysis',
                'LLVMTarget', 'LLVMMC', 'LLVMCore', 'LLVMSupport',
                'LLVMSystem',
            ])
        env.Append(LIBS = [
            'imagehlp',
            'psapi',
        ])
        if env['msvc']:
            # Some of the LLVM C headers use the inline keyword without
            # defining it.
            env.Append(CPPDEFINES = [('inline', '__inline')])
            if env['debug']:
                # LLVM libraries are static, build with /MT, and they
                # automatically link agains LIBCMT. When we're doing a
                # debug build we'll be linking against LIBCMTD, so disable
                # that.
                env.Append(LINKFLAGS = ['/nodefaultlib:LIBCMT'])
    elif env.Detect('llvm-config'):
        llvm_version = env.backtick('llvm-config --version').rstrip()

        try:
            env.ParseConfig('llvm-config --cppflags')
            env.ParseConfig('llvm-config --libs jit interpreter nativecodegen bitwriter')
            env.ParseConfig('llvm-config --ldflags')
        except OSError:
            print 'llvm-config version %s failed' % llvm_version
        else:
            env['LINK'] = env['CXX']
    else:
        return

    assert llvm_version is not None

    print 'scons: Found LLVM version %s' % llvm_version
    llvm_version = distutils.version.LooseVersion(llvm_version)
    env['LLVM_VERSION'] = llvm_version

    # Define HAVE_LLVM macro with the major/minor version number (e.g., 0x0206 for 2.6)
    llvm_version_major = int(llvm_version.version[0])
    llvm_version_minor = int(llvm_version.version[1])
    llvm_version_hex = '0x%02x%02x' % (llvm_version_major, llvm_version_minor)
    env.Prepend(CPPDEFINES = [('HAVE_LLVM', llvm_version_hex)])

def exists(env):
    return True

# vim:set ts=4 sw=4 et:
