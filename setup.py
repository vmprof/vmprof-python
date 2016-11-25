from setuptools import setup, find_packages, Extension
import os, sys

IS_PYPY = '__pypy__' in sys.builtin_module_names

if IS_PYPY:
    ext_modules = [] # built-in
else:
    libraries = []
    extra_compile_args = []
    if sys.platform != 'win32':
        extra_compile_args = ['-Wno-unused']
    elif sys.platform == 'darwin':
        libraries = ['unwind']
    extra_compile_args.append('-DNATIVE_FRAMES=1')
    ext_modules = [Extension('_vmprof',
                           sources=[
                               'src/_vmprof.c',
                               'src/stack.c',
                               #'src/hotpatch/tramp.c',
                               #'src/hotpatch/util.c',
                               #'src/hotpatch/elf.c',
                               #'src/hotpatch/x86_64.c',
                               #'src/hotpatch/x86_gen.c',
                               ],
                           depends=[
                               'src/vmprof_main.h',
                               'src/vmprof_main_32.h',
                               'src/vmprof_mt.h',
                               'src/vmprof_common.h',
                           ],
                            extra_compile_args=extra_compile_args,
                            libraries=['unwind','unwind-x86_64'])] # 'elf','dwarf',

if sys.version_info[:2] >= (3, 3):
    extra_install_requires = []
else:
    extra_install_requires = ["backports.shutil_which"]

setup(
    name='vmprof',
    author='vmprof team',
    author_email='fijal@baroquesoftware.com',
    version="0.4.0.dev0",
    packages=find_packages(),
    description="Python's vmprof client",
    long_description='See https://vmprof.readthedocs.org/',
    url='https://github.com/vmprof/vmprof-python',
    install_requires=[
        'requests',
        'six',
    ] + extra_install_requires,
    tests_require=['pytest','cffi'],
    entry_points = {
        'console_scripts': [
            'vmprofshow = vmprof.show:main'
    ]},
    classifiers=[
        'License :: OSI Approved :: BSD License',
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
