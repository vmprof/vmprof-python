Frequently Asked Questions
==========================

* **What does <native symbol 0xdeadbeef> mean?**: Debugging information might or might not be compiled
  with some libraries. If you see lots of those entries you might want to compile the libraries to include
  dwarf debugging information. In most cases ``gcc -g ...`` will help.
  If the symbol has been exported in the shared object (on linux), ``dladdr`` might still be able to extract
  the function name even if no debugging information has been attached.

* **What do the colors on vmprof.com mean?**: For plain CPython there is no particular meaning, we might change
  that in the future. For PyPy we have a color coding to show at which state the VM sampled (e.g. JIT, Warmup, ...).

* **My Windows profile is malformed?**: Please ensure that you open the file in binary mode. Otherwise Windows
  will transform `\n` to `\r\n`.

* **Do I need to install libunwind?**: Usually not. We ship python wheels that bundle libunwind shared objects. If you install vmprof from source, then you need to install the development headers of your distribution. OSX ships libunwind per default. If your pip version is really old it does not pull wheels and it will end up compiling from source.

