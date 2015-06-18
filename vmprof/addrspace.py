from __future__ import print_function
import bisect
import six


def fmtaddr(x, name=None):
    if name:
        return '0x%016x:%s' % (x, name)
    else:
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
        for k, v in six.iteritems(meta_data):
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
            return fmtaddr(addr, lib.name), addr, False, None
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
        # XXX this function is too complicated and too pypy specific
        #     refactor somehow
        filtered_profiles = []
        jit_frames = set()
        addr_set = set()
        for i, prof in enumerate(profiles):
            current = []
            first_virtual = False
            jitting = False
            for j, addr in enumerate(prof[0]):
                orig_addr = addr
                name, addr, is_virtual, lib = self.lookup(addr)
                if interp_name == 'pypy':
                    if orig_addr + 1 == 0x2:
                        jitting = True
                        added_anything = False
                        prev_name, jit_addr, _, _ = self.lookup(prof[0][j - 1])
                        current.append(jit_addr)
                        jit_frames.add(jit_addr)
                        continue
                    elif orig_addr + 1 == 0x3:
                        assert jitting
                        #if added_anything:
                        #    current.pop() # the frame is duplicated
                        jitting = False
                        continue
                if extra_info and addr in self.meta_data and not first_virtual:
                    # XXX hack for pypy - gc:minor calling asm_stackwalk
                    #     is just gc minor
                    if self.meta_data[addr].startswith('meta:gc') and current:
                        current = []
                    for item in current:
                        # sanity check if we're not double-counting,
                        # we need to change meta data setting if we are
                        if self.meta_data.get(item, None) == self.meta_data[addr]:
                            break # we can have blackhole in blackhole
                            # or whatever
                    else:
                        current.append(addr)
                elif is_virtual or not only_virtual:
                    first_virtual = True
                    added_anything = True
                    current.append(addr)
                    addr_set.add(addr)
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1]))
        return filtered_profiles, addr_set, jit_frames

    def dump_stack(self, stacktrace):
        for addr in stacktrace:
            print(fmtaddr(addr), self.lookup(addr)[0])


