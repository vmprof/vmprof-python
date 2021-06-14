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
$ pypy vmprof/test/cpuburn.py # you can find cpuburn.py in the vmprof-python repo
```

This will produce a profile file `vmprof_cpuburn.dat`.
Now display the profile using `vmprofshow`. `vmprofshow` has multiple modes
of showing data. We'll start with the tree-based mode.

### Tree-based output

```console
$ vmprofshow vmprof_cpuburn.dat tree
```

You will see a (colored) output:

```console
$ vmprofshow vmprof_cpuburn.dat tree
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

There is also an option ``--html`` to emit the same information as HTML to view
in a browser. In this case, the tree branches can be interactively expanded and
collapsed.

### Line-based output

vmprof supports line profiling mode, which enables collecting and showing the statistics for separate lines
inside functions.

To enable collection of lines statistics add `--lines` argument to vmprof:

```console
$ python -m vmprof --lines -o <output-file> <your program> <your program args>
```

Or pass `lines=True` argument to `vmprof.enable` function, when calling vmprof from code.

To see line statistics for all functions use the  `lines` mode of `vmprofshow`:
```console
$ vmprofshow <output-file> lines
```

To see line statistics for a specific function use the `--filter` argument with the function name:
```console
$ vmprofshow <output-file> lines --filter <function-name>
```

You will see the result:
```console
$ vmprofshow vmprof_cpuburn.dat lines --filter _next_rand
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

### "Flattened" output
`vmprofshow` also has a `flat` mode.

While the tree-based and line-based output styles for `vmprofshow` give a good
view of where time is spent when viewed from the 'root' of the call graph,
sometimes it is desirable to get a view from 'leaves' instead. This is particularly
helpful when functions exist that get called from multiple places, where each
invocation does not consume much time, but all invocations taken together do
amount to a substantial cost.
```console
$ vmprofshow vmprof_cpuburn.dat flat                                                                                                                                                                                                                                                                                                                                                                                   andreask_work@dunkel 15:24
    28.895% - _PyFunction_Vectorcall:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/call.c:389
    18.076% - _iterate:cpuburn.py:20
    17.298% - _next_rand:cpuburn.py:15
     5.863% - <native symbol 0x563a5f4eea51>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/longobject.c:3707
     5.831% - PyObject_SetAttr:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/object.c:1031
     4.924% - <native symbol 0x563a5f43fc01>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:787
     4.762% - PyObject_GetAttr:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/object.c:931
     4.373% - <native symbol 0x563a5f457eb1>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:1071
     3.758% - PyNumber_Add:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:957
     3.110% - <native symbol 0x563a5f47c291>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/longobject.c:4848
     1.587% - PyNumber_Multiply:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:988
     1.166% - _PyObject_GetMethod:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/object.c:1139
     0.356% - <native symbol 0x563a5f4ed8f1>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/longobject.c:3432
     0.000% - <native symbol 0x7f0dce8cca80>:-:0
     0.000% - test:cpuburn.py:36
     0.000% - burn:cpuburn.py:27
```
Sometimes it may be desirable to exclude "native" functions:
```console
$ vmprofshow vmprof_cpuburn.dat flat --no-native                                                                                                                                                                                                                                                                                                                                                                       andreask_work@dunkel 15:27
    53.191% - _next_rand:cpuburn.py:15
    46.809% - _iterate:cpuburn.py:20
     0.000% - test:cpuburn.py:36
     0.000% - burn:cpuburn.py:27
```
Note that the output represents the time spent in each function, *exclusive* of
functions called. (In `--no-native` mode, native-code callees remain included
in the total.)

Sometimes it may also be desirable to get timings *inclusive* of called functions:
```
$ vmprofshow vmprof_cpuburn.dat flat --include-callees                                                                                                                                                                                                                                                                                                                                                                 andreask_work@dunkel 15:31
   100.000% - <native symbol 0x7f0dce8cca80>:-:0
   100.000% - test:cpuburn.py:36
   100.000% - burn:cpuburn.py:27
   100.000% - _iterate:cpuburn.py:20
    53.191% - _next_rand:cpuburn.py:15
    28.895% - _PyFunction_Vectorcall:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/call.c:389
     7.807% - PyNumber_Multiply:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:988
     7.483% - <native symbol 0x563a5f457eb1>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:1071
     6.220% - <native symbol 0x563a5f4eea51>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/longobject.c:3707
     5.831% - PyObject_SetAttr:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/object.c:1031
     4.924% - <native symbol 0x563a5f43fc01>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:787
     4.762% - PyObject_GetAttr:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/object.c:931
     3.758% - PyNumber_Add:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/abstract.c:957
     3.110% - <native symbol 0x563a5f47c291>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/longobject.c:4848
     1.166% - _PyObject_GetMethod:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/object.c:1139
     0.356% - <native symbol 0x563a5f4ed8f1>:/home/conda/feedstock_root/build_artifacts/python-split_1608956461873/work/Objects/longobject.c:3432
```
This view is quite similar to the "tree" view, minus the nesting.
