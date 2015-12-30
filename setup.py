from setuptools import setup, find_packages, Extension
import os, sys

if '__pypy__' in sys.builtin_module_names:
    ext_modules = [] # built-in
else:
    ext_modules = [Extension('_vmprof',
                           sources=[
                               'src/_vmprof.c',
                               ],
                            extra_compile_args=['-Wno-unused'],
                            libraries=[])]
   

setup(
    name='vmprof',
    author='vmprof team',
    author_email='fijal@baroquesoftware.com',
    version="0.1.5.5",
    packages=find_packages(),
    description="Python's vmprof client",
    install_requires=[
        'six',
        'click'
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
