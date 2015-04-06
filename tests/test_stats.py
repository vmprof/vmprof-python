
import py
import vmprof, json, time, zlib
from vmprof.reader import LibraryData

def get_or_write_libcache(filename):
    libache_filename = str(py.path.local(__file__).join('..', filename + '.libcache'))
    try:
        with open(libache_filename) as f:
            d = json.loads(zlib.decompress(f.read()))
        lib_cache = {}
        for k, v in d.iteritems():
            lib_cache[k] = LibraryData(v[0], v[1], v[2], v[3], v[4])
        return lib_cache
    except (IOError, OSError):
        pass
    path = py.path.local(__file__).join('..', filename)
    lib_cache = {}
    vmprof.read_profile(path, virtual_only=True,
                        include_extra_info=True, lib_cache=lib_cache)
    d = {}
    for k, lib in lib_cache.iteritems():
        d[k] = (lib.name, lib.start, lib.end, lib.is_virtual, lib.symbols)
    with open(libache_filename, "w") as f:
        f.write(zlib.compress(json.dumps(d)))
    return lib_cache

def test_read_simple():
    lib_cache = get_or_write_libcache('simple_nested.pypy.prof')
    path = py.path.local(__file__).join('..', 'simple_nested.pypy.prof')
    stats = vmprof.read_profile(path, virtual_only=True,
                                include_extra_info=True, lib_cache=lib_cache)
    tree = stats.get_tree()
    foo_name == 'py:foo:6:foo.py'
    bar_name = 'py:bar:2:foo.py'
    import pdb
    pdb.set_trace()
    tree.flatten()
