from setuptools import setup, find_packages, Extension
from setuptools.command.build_py import build_py
from setuptools.command.build_ext import build_ext
import os, sys
import subprocess

IS_PYPY = '__pypy__' in sys.builtin_module_names

BASEDIR = os.path.dirname(os.path.abspath(__file__))

class vmprof_build(build_py, object):
    def run(self):
        super(vmprof_build, self).run()

class vmprof_build_ext(build_ext, object):
    """build_ext that runs libbacktrace configure before building.
    This is needed because libbacktrace does not have a pre-built library for all platforms.
    """
    def run(self):
        # configure libbacktrace on Unix systems (not Windows/macOS)
        if sys.platform.startswith('linux') or sys.platform.startswith('freebsd'):
            libbacktrace_dir = os.path.join(BASEDIR, "src", "libbacktrace")
            config_h = os.path.join(libbacktrace_dir, "config.h")
            # only run configure if config.h doesn't exist
            if not os.path.exists(config_h):
                orig_dir = os.getcwd()
                os.chdir(libbacktrace_dir)
                try:
                    # generate configure script if it doesn't exist
                    if not os.path.exists("configure"):
                        subprocess.check_call(["autoreconf", "-i"])
                    subprocess.check_call(["./configure"])
                finally:
                    os.chdir(orig_dir)
        super(vmprof_build_ext, self).run()

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

    else:
        raise NotImplementedError("platform '%s' is not supported!" % sys.platform)
    # use absolute paths for include directories so compilation works from any directory
    extra_compile_args.append('-I' + os.path.join(BASEDIR, 'src'))
    extra_compile_args.append('-I' + os.path.join(BASEDIR, 'src', 'libbacktrace'))
    if sys.version_info[:2] >= (3,11):
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
    cmdclass={'build_py': vmprof_build, 'build_ext': vmprof_build_ext},
    install_requires=[
        'requests',
        'six',
        'pytz',
        'colorama',
    ] + extra_install_requires,
    python_requires='<3.15',
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