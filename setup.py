from setuptools import setup, find_packages, Extension
import os, sys

IS_PYPY = '__pypy__' in sys.builtin_module_names

if IS_PYPY:
    ext_modules = [] # built-in
else:
    extra_compile_args = []
    extra_source_files = [
       'src/libudis86/decode.c',
       'src/libudis86/itab.c',
       'src/libudis86/udis86.c',
       'src/trampoline.c',
       'src/symboltable.c',
    ]
    if sys.platform == 'win32':
        extra_source_files = [
            'src/vmprof_main_win32.c',
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
    elif sys.platform.startswith('linux'):
        libraries = ['dl','unwind']
        extra_compile_args = ['-Wno-unused']
        extra_compile_args += ['-DVMPROF_LINUX=1']
        extra_compile_args += ['-DVMPROF_UNIX=1']
        if sys.maxsize == 2**63-1:
            libraries.append('unwind-x86_64')
        else:
            libraries.append('unwind-x86')
        extra_source_files += [
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
    extra_compile_args.append('-I src/')
    extra_compile_args.append('-I src/libbacktrace')
    ext_modules = [Extension('_vmprof',
                           sources=[
                               'src/_vmprof.c',
                               'src/machine.c',
                               'src/compat.c',
                               'src/vmp_stack.c',
                               ] + extra_source_files,
                           depends=[
                               'src/vmprof_main.h',
                               'src/vmprof_main_32.h',
                               'src/vmprof_mt.h',
                               'src/vmprof_common.h',
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
    version="0.4.0",
    packages=find_packages(),
    description="Python's vmprof client",
    long_description='See https://vmprof.readthedocs.org/',
    url='https://github.com/vmprof/vmprof-python',
    install_requires=[
        'requests',
        'six',
        'pytz',
        'colorama',
    ] + extra_install_requires,
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
