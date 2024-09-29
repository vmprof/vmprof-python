from setuptools import setup, find_packages, Extension
from distutils.command.build_py import build_py
import os, sys
import subprocess
import platform

IS_PYPY = '__pypy__' in sys.builtin_module_names

class vmprof_build(build_py, object):
    def run(self):
        super(vmprof_build, self).run()

BASEDIR = os.path.dirname(os.path.abspath(__file__))

def _supported_unix():
    if sys.platform.startswith('linux'):
        return 'linux'
    if sys.platform.startswith('freebsd'):
        return 'bsd'
    return False

if IS_PYPY:
    ext_modules = [] # built-in
else:
    extra_compile_args = []
    extra_source_files = [
       'src/symboltable.c',
    ]
    if sys.platform == 'win32':
        extra_source_files = [
            'src/vmprof_win.c',
        ] # remove the native source files
        libraries = []
        extra_compile_args = ['-DVMPROF_WINDOWS=1']
    elif sys.platform == 'darwin':
        libraries = []
        extra_compile_args = ['-Wno-unused']
        extra_compile_args += ['-DVMPROF_APPLE=1']
        extra_compile_args += ['-DVMPROF_UNIX=1']
        # overwrite the optimization level, if it is not optimized enough,
        # it might use the regiter rbx...
        extra_compile_args += ['-g']
        extra_compile_args += ['-O2']
        extra_source_files += ['src/vmprof_unix.c', 'src/vmprof_mt.c']
    elif _supported_unix():
        libraries = ['dl','unwind']
        extra_compile_args = ['-Wno-unused']
        if _supported_unix() == 'linux':
            extra_compile_args += ['-DVMPROF_LINUX=1']
        if _supported_unix() == 'bsd':
            libraries = ['unwind']
            extra_compile_args += ['-DVMPROF_BSD=1']
            extra_compile_args += ['-I/usr/local/include']
        extra_compile_args += ['-DVMPROF_UNIX=1']
        extra_source_files += [
           'src/vmprof_mt.c',
           'src/vmprof_unix.c',
           'src/libbacktrace/backtrace.c',
           'src/libbacktrace/state.c',
           'src/libbacktrace/elf.c',
           'src/libbacktrace/dwarf.c',
           'src/libbacktrace/fileline.c',
           'src/libbacktrace/mmap.c',
           'src/libbacktrace/mmapio.c',
           'src/libbacktrace/posix.c',
           'src/libbacktrace/sort.c',
        ]
        # configure libbacktrace!!
        class vmprof_build(build_py, object):
            def run(self):
                orig_dir = os.getcwd()
                os.chdir(os.path.join(BASEDIR, "src", "libbacktrace"))
                subprocess.check_call(["./configure"])
                os.chdir(orig_dir)
                super(vmprof_build, self).run()

    else:
        raise NotImplementedError("platform '%s' is not supported!" % sys.platform)
    extra_compile_args.append('-I src/')
    extra_compile_args.append('-I src/libbacktrace')
    if sys.version_info[:2] == (3,11):
        extra_source_files += ['src/populate_frames.c']
    ext_modules = [Extension('_vmprof',
                           sources=[
                               'src/_vmprof.c',
                               'src/machine.c',
                               'src/compat.c',
                               'src/vmp_stack.c',
                               'src/vmprof_common.c',
                               'src/vmprof_memory.c',
                               ] + extra_source_files,
                           depends=[
                               'src/vmprof_unix.h',
                               'src/vmprof_mt.h',
                               'src/vmprof_common.h',
                               'src/vmp_stack.h',
                               'src/symboltable.h',
                               'src/machine.h',
                               'src/vmprof.h',
                               'src/vmprof_memory.h',
                           ],
                           extra_compile_args=extra_compile_args,
                           libraries=libraries)]

if sys.version_info[:2] >= (3, 3):
    extra_install_requires = []
else:
    extra_install_requires = ["backports.shutil_which"]

setup(
    name='vmprof',
    author='vmprof team',
    author_email='fijal@baroquesoftware.com',
    version="0.4.18.1",
    packages=find_packages(),
    description="Python's vmprof client",
    long_description='See https://vmprof.readthedocs.org/',
    url='https://github.com/vmprof/vmprof-python',
    cmdclass={'build_py': vmprof_build},
    install_requires=[
        'requests',
        'six',
        'pytz',
        'colorama',
    ] + extra_install_requires,
    python_requires='<3.12',
    tests_require=['pytest','cffi','hypothesis'],
    entry_points = {
        'console_scripts': [
            'vmprofshow = vmprof.show:main'
    ]},
    classifiers=[
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: Implementation :: CPython',
        'Programming Language :: Python :: Implementation :: PyPy',
    ],
    zip_safe=False,
    include_package_data=True,
    ext_modules=ext_modules,
)
