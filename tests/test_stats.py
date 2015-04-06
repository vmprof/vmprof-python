
import py
import vmprof, json, time, zlib
from vmprof.reader import LibraryData

def serialize_lib_cache():
    path = py.path.local(__file__).join('..', 'richards.pypy.prof')
    lib_cache = {}
    vmprof.read_profile(path, virtual_only=True,
                        include_extra_info=True, lib_cache=lib_cache)
    d = {}
    for k, lib in lib_cache.iteritems():
        d[k] = (lib.name, lib.start, lib.end, lib.is_virtual, lib.symbols)
    with open(str(py.path.local(__file__).join('..', 'libcache')), "w") as f:
        f.write(zlib.compress(json.dumps(d)))

if __name__ == '__main__':
    serialize_lib_cache()

def get_lib_cache():
    with open(str(py.path.local(__file__).join('..', 'libcache'))) as f:
        d = json.loads(zlib.decompress(f.read()))
    lib_cache = {}
    for k, v in d.iteritems():
        lib_cache[k] = LibraryData(v[0], v[1], v[2], v[3], v[4])
    return lib_cache

def test_read_simple():
    py.test.skip("for now")
    lib_cache = get_lib_cache()
    path = py.path.local(__file__).join('..', 'simple_nested.pypy.prof')
    stats = vmprof.read_profile(path, virtual_only=True,
                                include_extra_info=True, lib_cache=lib_cache)
    tree = stats.get_tree()
    import pdb
    pdb.set_trace()
    tree.flatten()
