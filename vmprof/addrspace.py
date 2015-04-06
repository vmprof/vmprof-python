import bisect

def fmtaddr(x):
    return '0x%016x' % x


class AddressSpace(object):
    def __init__(self, libs):
        all = [(lib.start, lib) for lib in libs]
        all.sort()
        self.libs = [lib for _, lib in all]
        self.lib_lookup = [lib.start for lib in self.libs]
        # pypy metadata
        meta_data = {
            'pypy_g_resume_in_blackhole': 'meta:blackhole',
            'pypy_g_MetaInterp__compile_and_run_once': 'meta:tracing',
            'pypy_g_ResumeGuardDescr__trace_and_compile_from_bridge': 'meta:tracing',
            'pypy_g_IncrementalMiniMarkGC_major_collection_step': 'meta:gc:major',
            'pypy_asm_stackwalk': 'meta:external',
            'pypy_g_IncrementalMiniMarkGC_minor_collection': 'meta:gc:minor',
            
        }
        self.meta_data = {}
        for k, v in meta_data.iteritems():
            keys = self.reverse_lookup(k)
            for key in keys:
                self.meta_data[key] = v

    def lookup(self, arg):
        addr = arg + 1
        i = bisect.bisect(self.lib_lookup, addr)
        if i > len(self.libs) or i <= 0:
            return fmtaddr(addr), addr, False, None
        lib = self.libs[i - 1]
        if addr < lib.start or addr >= lib.end:
            return fmtaddr(addr), addr, False, None
        i = bisect.bisect(lib.symbols, (addr + 1,))
        if i > len(lib.symbols) or i <= 0:
            return fmtaddr(addr), addr, False, None
        addr, name = lib.symbols[i - 1]
        is_virtual = lib.is_virtual
        return name, addr, is_virtual, lib

    def reverse_lookup(self, name):
        l = []
        for lib in self.libs:
            for no, sym in lib.symbols:
                if name in sym: # remember about .part.x
                    l.append(no)
        return l

    def filter(self, profiles):
        filtered_profiles = []
        for prof in profiles:
            current = []
            for addr in prof[0]:
                name, true_addr, is_virtual, _ = self.lookup(addr)
                if is_virtual:
                    current.append(name)
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1]))
        return filtered_profiles

    def filter_addr(self, profiles, only_virtual=True, extra_info=False,
                    interp_name=None):
        def qq(count=10):
            import pprint
            pprint.pprint([self.lookup(a)[0] for a in prof[0]][:count])

        def hh(count=10):
            import pprint
            pprint.pprint([self.lookup(a)[0] for a in current[:count]]) 
           
        
        filtered_profiles = []
        jit_frames = set()
        addr_set = set()
        pypy = interp_name == 'pypy'
        last_was_jitted = False
        for i, prof in enumerate(profiles):
            current = []
            first_virtual = False
            jitted = False
            for j, addr in enumerate(prof[0]):
                orig_addr = addr
                name, addr, is_virtual, lib = self.lookup(addr)
                if extra_info and addr in self.meta_data and not first_virtual:
                    for item in current:
                        # sanity check if we're not double-counting,
                        # we need to change meta data setting if we are
                        if self.meta_data[item] == self.meta_data[addr]:
                            break # we can have blackhole in blackhole
                            # or whatever
                    else:
                        current.append(addr)
                elif is_virtual or not only_virtual:
                    first_virtual = True
                    if (orig_addr + 1) & 1 == 0 and pypy and not jitted: # jitted
                        prev_name, jit_addr, _, _ = self.lookup(prof[0][j - 1])
                        assert prev_name != 'pypy_pyframe_execute_frame'
                        assert not prev_name.startswith('py:')
                        jitted = True
                        current.append(jit_addr)
                        jit_frames.add(jit_addr)
                    if (orig_addr + 1) & 1 == 1 and current and current[-1] == addr and last_was_jitted:
                        # there is double foo(), one jitted one not, strip one
                        last_was_jitted = False
                        continue
                    if (orig_addr + 1) & 1 == 0:
                        last_was_jitted = True
                    else:
                        last_was_jitted = False
                    current.append(addr)
                    addr_set.add(addr)
                if not is_virtual:
                    jitted = False
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1]))
        return filtered_profiles, addr_set, jit_frames

    def dump_stack(self, stacktrace):
        for addr in stacktrace:
            print fmtaddr(addr), self.lookup(addr)[0]


