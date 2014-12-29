import bisect


def fmtaddr(x):
    return '0x%016x' % x


class AddressSpace(object):
    def __init__(self, libs):
        all = [(lib.start, lib) for lib in libs]
        all.sort()
        self.libs = [lib for _, lib in all]
        self.lib_lookup = [lib.start for lib in self.libs]

    def lookup(self, arg):
        addr = arg + 1
        i = bisect.bisect(self.lib_lookup, addr)
        if i > len(self.libs) or i <= 0:
            return fmtaddr(addr), False
        lib = self.libs[i - 1]
        if addr < lib.start or addr >= lib.end:
            return fmtaddr(addr), False
        i = bisect.bisect(lib.symbols, (addr + 1,))
        if i > len(lib.symbols) or i <= 0:
            return fmtaddr(addr), False
        addr, name = lib.symbols[i - 1]
        is_virtual = lib.is_virtual
        return name, is_virtual

    def filter(self, profiles):
        filtered_profiles = []
        for prof in profiles:
            current = []
            for addr in prof[0]:
                name, is_virtual = self.lookup(addr)
                if is_virtual:
                    current.append(name)
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1]))
        return filtered_profiles

    def filter_addr(self, profiles):
        filtered_profiles = []
        addr_set = set()
        for prof in profiles:
            current = []
            for addr in prof[0]:
                name, is_virtual = self.lookup(addr)
                if is_virtual:
                    new_addr = addr & (~0x8000000000000000L)
                    current.append(new_addr)
                    addr_set.add(new_addr)
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1]))
        return filtered_profiles, addr_set

    def dump_stack(self, stacktrace):
        for addr in stacktrace:
            print fmtaddr(addr), self.lookup(addr)[0]


class Profiles(object):
    def __init__(self, profiles):
        self.profiles = profiles
        self.functions = {}
        self.generate_top()

    def generate_top(self):
        for profile in self.profiles:
            current_iter = {}
            for name in profile[0]:
                if name not in current_iter:  # count only topmost
                    self.functions[name] = self.functions.get(name, 0) + 1
                    current_iter[name] = None

    def generate_per_function(self, top_function):
        """ Show functions that we call (directly or indirectly) under
        a given name
        """
        result = {}
        total = 0
        for profile in self.profiles:
            current_iter = {}  # don't count twice
            counting = False
            for name in profile[0]:
                if counting:
                    if name in current_iter:
                        continue
                    current_iter[name] = None
                    result[name] = result.get(name, 0) + 1
                else:
                    if name == top_function:
                        counting = True
                        total += 1
        return result, total
