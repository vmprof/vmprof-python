from __future__ import print_function
import bisect
import six
import warnings


def fmtaddr(x, name=None):
    if name:
        return '0x%016x:%s' % (x, name)
    else:
        return '0x%016x' % x

class Hashable(object):
    def __init__(self, addr):
        self.addr = addr

    def __hash__(self):
        return hash(self.addr)

    def __eq__(self, other):
        if isinstance(other, Hashable):
            return self.addr == other.addr
        return self.addr == other

    def __ne__(self, other):
        return not self == other

    def __repr__(self):
        return '<%s(%s)>' % (self.__class__.__name__, self.addr)
    
class JitAddr(Hashable):
    def jitted_version_of(self, addr):
        return False

class JittedVirtual(Hashable):
    pass

class VirtualFrame(Hashable):
    pass

class BaseMetaFrame(Hashable):
    def add_to_meta(self, meta):
        meta[self.name] = meta.get(self.name, 0) + 1

class GCFrame(BaseMetaFrame):
    pass

class MinorGCFrame(GCFrame):
    name = 'gc:minor'

class MajorGCFrame(GCFrame):
    name = 'gc:major'

class WarmupFrame(BaseMetaFrame):
    name = 'tracing'

class BlackholeWarmupFrame(WarmupFrame):
    name = 'blackhole'

class AddressSpace(object):
    def __init__(self, libs):
        all = [(lib.start, lib) for lib in libs]
        all.sort()
        self.libs = [lib for _, lib in all]
        self.lib_lookup = [lib.start for lib in self.libs]
        # pypy metadata
        meta_data = {
            'pypy_g_resume_in_blackhole': BlackholeWarmupFrame,
            'pypy_g_MetaInterp__compile_and_run_once': WarmupFrame,
            'pypy_g_ResumeGuardDescr__trace_and_compile_from_bridge': WarmupFrame,
            'pypy_g_IncrementalMiniMarkGC_major_collection_step': MajorGCFrame,
            'pypy_g_IncrementalMiniMarkGC_minor_collection': MinorGCFrame,

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

    def _next_profile(self, lst, jit_frames, addr_set, interp_name,
                      only_virtual):
        current = []
        jitting = False
        previous_virtual = None
        for j, addr in enumerate(lst):
            orig_addr = addr
            name, addr, is_virtual, lib = self.lookup(addr)
            if interp_name == 'pypy':
                if orig_addr + 1 == 0x2:
                    jitting = True
                    prev_name, jit_addr, _, _ = self.lookup(lst[j - 1])
                    # pop the previous python function if it's the same
                    # as the next one
                    # XXXX this is wrong, we should mark more frames
                    addr_set.add(jit_addr)
                    continue
                elif orig_addr + 1 == 0x3:
                    assert jitting
                    current.append(JitAddr(jit_addr))
                    jitting = False
                    continue
            if addr in self.meta_data:
                current.append(self.meta_data[addr](addr))
            elif is_virtual or not only_virtual:
                if jitting:
                    cls = JittedVirtual
                else:
                    cls = VirtualFrame
                if previous_virtual != addr:
                    current.append(cls(addr))
                    previous_virtual = current[-1]
                addr_set.add(addr)
        return current

    def filter_addr(self, profiles, only_virtual=True,
                    interp_name=None):
        filtered_profiles = []
        jit_frames = set()
        addr_set = set()
        skipped = 0
        for i, prof in enumerate(profiles):
            if len(prof[0]) < 5 or prof[0][-1] == 0x3:
                skipped += 1
                continue # broken profile
            current = self._next_profile(prof[0], jit_frames, addr_set,
                                         interp_name, only_virtual)
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1], prof[2]))
        if len(filtered_profiles) < 10:
            warnings.warn("there are only %d profiles, data will be unreliable" % (len(filtered_profiles),))
        if skipped > 0.05 * len(filtered_profiles):
            warnings.warn("there are %d broken profiles out of %d, data will be unreliable" % (skipped, len(filtered_profiles)))
        return filtered_profiles, addr_set, jit_frames

    def dump_stack(self, stacktrace):
        for addr in stacktrace:
            print(fmtaddr(addr), self.lookup(addr)[0])


