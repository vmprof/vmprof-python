Frequently Asked Questions
==========================

* **What does <native symbol 0xdeadbeef> mean?**: Debugging information might or might not be compiled
  with some libraries. If you see lots of those entries you might want to compile the libraries to include
  dwarf debugging information. In most cases ``gcc -g ...`` will help.

* **What do the colors on vmprof.com mean?**: For plain CPython there is no particular meaning, we might change
  that in the future. For PyPy we have a color coding to show at which state the VM sampled (e.g. JIT, Warmup, ...).
