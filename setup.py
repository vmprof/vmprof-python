from setuptools import setup, find_packages, Extension
import os

j = os.path.join

libdwarf = j(j(j(os.path.dirname(os.path.abspath(__file__)), 'src'),
             'hotpatch'), 'libdwarf.a')

setup(
    name='vmprof',
    author='vmprof team',
    author_email='sebastian.pawlus@gmail.com',
    version="0.0.3",
    packages=find_packages(),
    description="Python's vmprof client",
    install_requires=[
        "requests==2.5.1"
    ],
    classifiers=[
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python',
    ],
    zip_safe=False,
    ext_modules=[Extension('_vmprof',
                           sources=[
                               'src/_vmprof.c',
                               'src/hotpatch/tramp.c',
                               'src/hotpatch/elf.c',
                               'src/hotpatch/x86_gen.c',
                               'src/hotpatch/util.c',
                               'src/vmprof.c',
                               ],
                            libraries=['elf', 'unwind'],
                            extra_link_args=['%s' % libdwarf])],
)
