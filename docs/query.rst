Jit Log Query Interface
=======================

This command line interface can be used to pretty print optimized
program parts. If you are unfamiliar with the JIT compiler
built into PyPy, we highly recommend reading the `docs`_ for it.

.. _`docs`: https://rpython.readthedocs.io/en/latest/jit/index.html

Security Warning
----------------

It is discouraged to run the query API on a remote server.
As soon as the query parameter (`-q`) is parameterized, arbitrary
code execution can be performed.
Note that this is fine as long one can trust the user.

Basic Usage
-----------

Let's go ahead and inspect the `example.py` program in this repository.
It is assumed that the reader setup vmprof for pypy already (e.g. in a
virtualenv).

Now run the following command to generate the log::

    # run your program and output the log
    pypy -m vmprof -o log.jit example.py

This generates the file that normally is sent to `vmprof.com`_ whenever
`--web` is provided.

The query interface is a the flag '-q' which incooperates a small
query language. Here is an example::

    pypy -m jitlog log.jit -q 'bridges & op("int_add_ovf")'
    ... # will print the filtered traces

.. _`vmprof.com`: http://vmprof.com

Query API
---------

Brand new. Subject to change!

.. function:: loops

    Filter: Reduces the output to loops only

.. function:: bridges

    Filter: Reduces the output to bridges only

.. function:: func(name)

    Filter: Selects a trace if it happens to optimize the function containing the name.

.. function:: op(name)

    Filter: Only selects a traces if it contains the IR operation name.
