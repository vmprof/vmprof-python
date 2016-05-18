from setuptools import setup, find_packages, Extension
import os, sys

if '__pypy__' in sys.builtin_module_names:
    ext_modules = [] # built-in
else:
    if sys.platform != 'win32':
        extra_compile_args = ['-Wno-unused']
    else:
        extra_compile_args = []
    ext_modules = [Extension('_vmprof',
                           sources=[
                               'src/_vmprof.c',
                               ],
                           depends=[
                               'src/vmprof_main.h',
                               'src/vmprof_main_32.h',
                               'src/vmprof_mt.h',
                               'src/vmprof_common.h',
                           ],
                            extra_compile_args=extra_compile_args,
                            libraries=['z'])]
   

setup(
    name='vmprof',
    author='vmprof team',
    author_email='fijal@baroquesoftware.com',
    version="0.2.7",
    packages=find_packages(),
    description="Python's vmprof client",
    long_description='See https://vmprof.readthedocs.org/',
    url='https://github.com/vmprof/vmprof-python',
    install_requires=[
        'six',
    ],
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
