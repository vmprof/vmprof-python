Frequently Asked Questions
==========================

* **What does <native symbol 0xdeadbeef> mean?**: Debugging information might or might not be compiled
  with some libraries. If you see lots of those entries you might want to compile the libraries to include
  dwarf debugging information. In most cases ``gcc -g ...`` will help.
  If the symbol has been exported in the shared object (on linux), ``dladdr`` might still be able to extract
  the function name even if no debugging information has been attached.

* **Is it possible to just profile a part of my program?**: Yes here an example how you could do just that::

    with open('profile.prof', 'w+b') as fd:
      vmprof.enable(fd.fileno())
      my_function_or_program()
      vmprof.disable()

  Then open ``profile.prof`` in any of the viewers described in
  :doc:`viewers`.

* **Which viewer should I use?**: ``vmprofshow`` is bundled and needs nothing
  installed, the Firefox Profiler gives you a flame graph and a timeline, and
  kcachegrind gives you caller and callee lists and a call graph. See
  :doc:`viewers`.

* **My Windows profile is malformed?**: Please ensure that you open the file in binary mode. Otherwise Windows
  will transform ``\n`` to ``\r\n``.

* **Do I need to install libunwind?**: Usually not. We ship python wheels that bundle libunwind shared objects. If you install vmprof from source, then you need to install the development headers of your distribution. OSX ships libunwind per default. If your pip version is really old it does not pull wheels and it will end up compiling from source.

* **Why are the call counts in kcachegrind all 1?**: Because vmprof samples
  the stack rather than instrumenting calls, so it never sees an individual
  call and cannot count them. See :doc:`viewers`.
