# VMProf Python package

[![Tests](https://github.com/vmprof/vmprof-python/actions/workflows/tests.yml/badge.svg)](https://github.com/vmprof/vmprof-python/actions/workflows/tests.yml)
[![Wheels](https://github.com/vmprof/vmprof-python/actions/workflows/cibuildwheel.yml/badge.svg)](https://github.com/vmprof/vmprof-python/actions/workflows/cibuildwheel.yml)
[![Read The Docs](https://readthedocs.org/projects/vmprof/badge/?version=latest)](https://vmprof.readthedocs.org/en/latest/)

**VMProf** is a lightweight statistical profiler for CPython and PyPy. It samples
the call stack of a running program and writes a profile file you can open in
several viewers.

Head over to https://vmprof.readthedocs.org for more info!

## Installation

```console
pip install vmprof
```

VMProf 0.6 supports CPython 3.10 through 3.14 and PyPy, on Linux, Mac OS X and
Windows. Native profiling is available on Linux and Mac OS X.

Wheels are published to PyPI for all three platforms with libunwind bundled in.
If you build from source you need the CPython development headers, and on Linux
the libunwind headers as well — on Debian or Ubuntu, `python3-dev` and
`libunwind-dev`. On Windows you need the Microsoft Visual C++ Compiler for your
Python version.

## Quick start

Record a profile:

```console
$ python -m vmprof -o profile.prof <your program> <your program args>
```

Then open `profile.prof` in whichever viewer fits the question you're asking:

| Viewer | Good for | How |
| --- | --- | --- |
| `vmprofshow` | a quick look, no extra installs | `vmprofshow profile.prof tree` |
| [Firefox Profiler](https://profiler.firefox.com) | flame graph, timeline | `python -m vmprofconvert -convert profile.prof` |
| [kcachegrind](https://kcachegrind.github.io/) | callers/callees, call graph | `vmprofshow profile.prof callgrind -o profile.callgrind` |

Running `python -m vmprof` without `-o` prints basic statistics and keeps no
file.

### Firefox Profiler

The [vmprof-firefox-converter](https://github.com/Cskorpion/vmprof-firefox-converter)
converts a profile into a format the Firefox Profiler UI reads, giving you a
flame graph, a stack chart over time and an inverted call tree in the browser.
It understands PyPy's JIT frames too — see
[the announcement post](https://pypy.org/posts/2024/05/vmprof-firefox-converter.html)
for a tour.

```console
$ python -m pip install vmprof-firefox-converter
$ python -m vmprofconvert -convert profile.prof
```

### kcachegrind

`vmprofshow` can write the profile in callgrind format, which kcachegrind (or
`qcachegrind` on Mac OS X and Windows) reads:

```console
$ vmprofshow profile.prof callgrind -o profile.callgrind
$ kcachegrind profile.callgrind
```

The exported event is `Periods`: each sample is weighted by the time since the
previous one, in units of the sampling period, so costs are proportional to time
spent. At the default ~1kHz one unit is about 0.99ms.

Since vmprof samples the stack rather than instrumenting calls, it has no call
counts — every call edge is written as `calls=1`, so ignore kcachegrind's call
count column. Self cost is attributed to the line a function is defined on; use
`vmprofshow profile.prof lines` when you need line-level numbers.

## Development

Setting up development can be done using the following commands:

    $ python3 -m venv vmprof3
    $ source vmprof3/bin/activate
    $ pip install meson-python meson ninja
    $ pip install --no-build-isolation --editable .

You need to install python development packages. In case of e.g. Debian or Ubuntu the package you need is `python3-dev` and `libunwind-dev`.

Run the tests with:

    $ pip install pytest cffi setuptools
    $ python -m pytest vmprof/

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
$ vmprofshow vmprof_cpuburn.dat flat
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
$ vmprofshow vmprof_cpuburn.dat flat --no-native
    53.191% - _next_rand:cpuburn.py:15
    46.809% - _iterate:cpuburn.py:20
     0.000% - test:cpuburn.py:36
     0.000% - burn:cpuburn.py:27
```
Note that the output represents the time spent in each function, *exclusive* of
functions called. (In `--no-native` mode, native-code callees remain included
in the total.)

Sometimes it may also be desirable to get timings *inclusive* of called functions:
```console
$ vmprofshow vmprof_cpuburn.dat flat --include-callees
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

### Callgrind output

See [kcachegrind](#kcachegrind) above.
