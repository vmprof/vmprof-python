# VMProf Python package

[![Build Status on TravisCI](https://travis-ci.org/vmprof/vmprof-python.svg?branch=master)](https://travis-ci.org/vmprof/vmprof-python)
[![Build Status on TeamCity](https://teamcity.jetbrains.com/app/rest/builds/buildType:(id:VMprofPython_TestsPy27Win)/statusIcon.svg)](https://teamcity.jetbrains.com/project.html?projectId=VMprofPython)
[![Read The Docs](https://readthedocs.org/projects/vmprof/badge/?version=latest)](https://vmprof.readthedocs.org/en/latest/)
[![Build Status on AppVeyor](https://ci.appveyor.com/api/projects/status/github/vmprof/vmprof-python?branch=master&svg=true)](https://ci.appveyor.com/project/planrich/vmprof-python)


Head over to https://vmprof.readthedocs.org for more info!

## Installation

```console
pip install vmprof
python -m vmprof <your program> <your program args>
```

Our build system ships wheels to PyPI (Linux, Mac OS X). If you build from source you need
to install CPython development headers and libunwind headers (on Linux only).
On Windows this means you need Microsoft Visual C++ Compiler for your Python version.

## Development

Setting up development can be done using the following commands:

	$ virtualenv -p /usr/bin/python3 vmprof3
	$ source vmprof3/bin/activate
	$ python setup.py develop

You need to install python development packages. In case of e.g. Debian or Ubuntu the package you need is `python3-dev` and `libunwind-dev`.
Now it is time to write a test and implement your feature. If you want
your changes to affect vmprof.com, head over to
https://github.com/vmprof/vmprof-server and follow the setup instructions.

Consult our section for development at https://vmprof.readthedocs.org for more
information.

## vmprofshow

`vmprofshow` is a command line tool that comes with **VMProf**. It can read profile files
and produce a formatted output.

Here is an example of how to use `vmprofshow`:

Run that smallish program which burns CPU cycles (with vmprof enabled):

```console
pypy vmprof/test/cpuburn.py # you can find cpuburn.py in the vmprof-python repo
```

This will produce a profile file `vmprof_cpuburn.dat`.
Now display the profile:

```console
vmprofshow vmprof_cpuburn.dat
```

You will see a (colored) output:

```console
oberstet@thinkpad-t430s:~/scm/vmprof-python$ vmprofshow vmprof_cpuburn.dat
100.0%  <module>  100.0%  tests/cpuburn.py:1
100.0% .. test  100.0%  tests/cpuburn.py:35
100.0% .... burn  100.0%  tests/cpuburn.py:26
 99.2% ...... _iterate  99.2%  tests/cpuburn.py:19
 97.7% ........ _iterate  98.5%  tests/cpuburn.py:19
 22.9% .......... _next_rand  23.5%  tests/cpuburn.py:14
 22.9% ............ JIT code  100.0%  0x7fa7dba57a10
 74.7% .......... JIT code  76.4%  0x7fa7dba57a10
  0.1% .......... JIT code  0.1%  0x7fa7dba583b0
  0.5% ........ _next_rand  0.5%  tests/cpuburn.py:14
  0.0% ........ JIT code  0.0%  0x7fa7dba583b0
```


## Line profiling

vmprof supports line profiling mode, which enables collecting and showing the statistics for separate lines
inside functions.

To enable collection of lines statistics add `--lines` argument to vmprof:

```console
python -m vmprof --lines -o <output-file> <your program> <your program args>
```

Or pass `lines=True` argument to `vmprof.enable` function, when calling vmprof from code.

To see line statistics for all functions add the `--lines` argument to `vmprofshow`:
```console
vmprofshow --lines <output-file>
```

To see line statistics for a specific function use the `--filter` argument with the function name:
```console
vmprofshow --lines --filter <function-name> <output-file>
```

You will see the result:
```console
macbook-pro-4:vmprof-python traff$ vmprofshow --lines --filter _next_rand vmprof_cpuburn.dat
Total hits: 1170 s
File: tests/cpuburn.py
Function: _next_rand at line 14

Line #     Hits   % Hits  Line Contents
=======================================
    14       38      3.2      def _next_rand(self):
    15                            # http://rosettacode.org/wiki/Linear_congruential_generator
    16      835     71.4          self._rand = (1103515245 * self._rand + 12345) & 0x7fffffff
    17      297     25.4          return self._rand
```

