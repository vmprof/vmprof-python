from vmprof.callgraph import SymbolicStackTrace, Frame
from vmprof.reader import LibraryData
from vmprof.addrspace import AddressSpace


def test_StackTrace():
    lib = LibraryData("libfoo", 1000, 1500, is_virtual=False,
                      symbols=[(1000, "one"),
                               (1100, "two"),
                               (1200, "three")
                           ])
    virtlib = LibraryData("<virtuals>", 2000, 2500, is_virtual=True,
                          symbols=[(2000, "py:main"),
                                   (2100, "py:func"),
                               ])
    addrspace = AddressSpace([lib, virtlib])
    #
    stacktrace = [0x123, 1005, 1210, 2005]
    stacktrace = SymbolicStackTrace(stacktrace, addrspace)
    assert len(stacktrace) == 4
    assert stacktrace[1] == Frame('one')
    assert stacktrace[-1] == Frame('py:main', tag='py')
    # XXX: why do addrspace.lookup return addr+1? Is it a feature or a bug? We
    # should document it
    assert repr(stacktrace) == '[0x0000000000000124, one, three, py:main]'

