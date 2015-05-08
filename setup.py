from setuptools import setup, find_packages, Extension
import os, sys

j = os.path.join

libdwarf = j(j(j(os.path.dirname(os.path.abspath(__file__)), 'src'),
             'hotpatch'), 'libdwarf.a')


if hasattr(sys, 'pypy_translation_info'):
    ext_modules = [] # built-in
else:
    ext_modules = [Extension('_vmprof',
                           sources=[
                               'src/_vmprof.c',
                               'src/hotpatch/tramp.c',
                               'src/hotpatch/elf.c',
                               'src/hotpatch/x86_gen.c',
                               'src/hotpatch/util.c',
                               'src/vmprof.c',
                               ],
                            extra_compile_args=['-Wno-unused',
                                                '-I/usr/include/elf',
                                                '-I/usr/include/libdwarf'],
                            libraries=['elf', 'unwind', 'rt'],
                            extra_link_args=['%s' % libdwarf])]


setup(
    name='vmprof',
    author='vmprof team',
    author_email='sebastian.pawlus@gmail.com',
    version="0.0.9",
    packages=find_packages(),
    description="Python's vmprof client",
    install_requires=[
    ],
    classifiers=[
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python',
    ],
    zip_safe=False,
    include_package_data=True,
    ext_modules=ext_modules,
)
