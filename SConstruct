#######################################################################
# Top-level SConstruct
#
# For example, invoke scons as 
#
#   scons debug=1 dri=0 machine=x86
#
# to set configuration variables. Or you can write those options to a file
# named config.py:
#
#   # config.py
#   debug=1
#   dri=0
#   machine='x86'
# 
# Invoke
#
#   scons -h
#
# to get the full list of options. See scons manpage for more info.
#  

import os
import os.path
import sys

import common

#######################################################################
# Configuration options

if common.default_platform in ('linux', 'freebsd', 'darwin'):
	default_statetrackers = 'mesa'
	default_drivers = 'softpipe,failover,i915simple,i965simple'
	default_winsys = 'xlib'
elif common.default_platform in ('winddk',):
	default_statetrackers = 'none'
	default_drivers = 'softpipe,i915simple'
	default_winsys = 'none'
else:
	default_drivers = 'all'
	default_winsys = 'all'

opts = Options('config.py')
common.AddOptions(opts)
opts.Add(ListOption('statetrackers', 'state_trackers to build', default_statetrackers,
                     ['mesa']))
opts.Add(ListOption('drivers', 'pipe drivers to build', default_drivers,
                     ['softpipe', 'failover', 'i915simple', 'i965simple', 'cell']))
opts.Add(ListOption('winsys', 'winsys drivers to build', default_winsys,
                     ['xlib', 'intel'])) 

env = Environment(
	options = opts, 
	ENV = os.environ)
Help(opts.GenerateHelpText(env))

# for debugging
#print env.Dump()

# replicate options values in local variables
debug = env['debug']
dri = env['dri']
llvm = env['llvm']
machine = env['machine']
platform = env['platform']

# derived options
x86 = machine == 'x86'
gcc = platform in ('linux', 'freebsd', 'darwin')
msvc = platform in ('win32', 'winddk')

Export([
	'debug', 
	'x86', 
	'dri', 
	'llvm',
	'platform',
	'gcc',
	'msvc',
])


#######################################################################
# Environment setup
#
# TODO: put the compiler specific settings in separate files
# TODO: auto-detect as much as possible


if platform == 'winddk':
	env.Tool('winddk', ['.'])
	
	env.Append(CPPPATH = [
		env['SDK_INC_PATH'],
		env['DDK_INC_PATH'],
		env['WDM_INC_PATH'],
		env['CRT_INC_PATH'],
	])

# Optimization flags
if gcc:
	if debug:
		env.Append(CFLAGS = '-O0 -g3')
		env.Append(CXXFLAGS = '-O0 -g3')
	else:
		env.Append(CFLAGS = '-O3 -g3')
		env.Append(CXXFLAGS = '-O3 -g3')

	env.Append(CFLAGS = '-Wall -Wmissing-prototypes -Wno-long-long -ffast-math -pedantic')
	env.Append(CXXFLAGS = '-Wall -pedantic')
	
	# Be nice to Eclipse
	env.Append(CFLAGS = '-fmessage-length=0')
	env.Append(CXXFLAGS = '-fmessage-length=0')

if msvc:
	env.Append(CFLAGS = '/W3')
	if debug:
		cflags = [
			'/Od', # disable optimizations
			'/Oy-', # disable frame pointer omission
		]
	else:
		cflags = [
			'/Ox', # maximum optimizations
			'/Os', # favor code space
		]
	env.Append(CFLAGS = cflags)
	env.Append(CXXFLAGS = cflags)
	# Put debugging information in a separate .pdb file for each object file as
	# descrived in the scons manpage
	env['CCPDBFLAGS'] = '/Zi /Fd${TARGET}.pdb'

# Defines
if debug:
	if gcc:
		env.Append(CPPDEFINES = ['DEBUG'])
	if msvc:
		env.Append(CPPDEFINES = [
			('DBG', '1'),
			('DEBUG', '1'),
			('_DEBUG', '1'),
		])
else:
	env.Append(CPPDEFINES = ['NDEBUG'])


# Includes
env.Append(CPPPATH = [
	'#/include',
	'#/src/gallium/include',
	'#/src/gallium/auxiliary',
	'#/src/gallium/drivers',
])


# x86 assembly
if x86:
	env.Append(CPPDEFINES = [
		'USE_X86_ASM', 
		'USE_MMX_ASM',
		'USE_3DNOW_ASM',
		'USE_SSE_ASM',
	])
	if gcc:	
		env.Append(CFLAGS = '-m32')
		env.Append(CXXFLAGS = '-m32')


# Posix
if platform in ('posix', 'linux', 'freebsd', 'darwin'):
	env.Append(CPPDEFINES = [
		'_POSIX_SOURCE',
		('_POSIX_C_SOURCE', '199309L'), 
		'_SVID_SOURCE',
		'_BSD_SOURCE', 
		'_GNU_SOURCE',
		
		'PTHREADS',
		'HAVE_POSIX_MEMALIGN',
	])
	env.Append(CPPPATH = ['/usr/X11R6/include'])
	env.Append(LIBPATH = ['/usr/X11R6/lib'])
	env.Append(LIBS = [
		'm',
		'pthread',
		'expat',
		'dl',
	])


# DRI
if dri:
	env.ParseConfig('pkg-config --cflags --libs libdrm')
	env.Append(CPPDEFINES = [
		('USE_EXTERNAL_DXTN_LIB', '1'), 
		'IN_DRI_DRIVER',
		'GLX_DIRECT_RENDERING',
		'GLX_INDIRECT_RENDERING',
	])

# LLVM
if llvm:
	# See also http://www.scons.org/wiki/UsingPkgConfig
	env.ParseConfig('llvm-config --cflags --ldflags --libs')
	env.Append(CPPDEFINES = ['MESA_LLVM'])
	env.Append(CXXFLAGS = ['-Wno-long-long'])
	

# libGL
if platform not in ('winddk',):
	env.Append(LIBS = [
		'X11',
		'Xext',
		'Xxf86vm',
		'Xdamage',
		'Xfixes',
	])

# Convenience library support
common.createConvenienceLibBuilder(env)

Export('env')


#######################################################################
# Invoke SConscripts

# TODO: Build several variants at the same time?
# http://www.scons.org/wiki/SimultaneousVariantBuilds

SConscript(
	'src/SConscript',
	build_dir = common.make_build_dir(env),
	duplicate = 0 # http://www.scons.org/doc/0.97/HTML/scons-user/x2261.html
)
