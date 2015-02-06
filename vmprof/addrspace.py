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
            return fmtaddr(addr), addr, False
        lib = self.libs[i - 1]
        if addr < lib.start or addr >= lib.end:
            return fmtaddr(addr), addr, False
        i = bisect.bisect(lib.symbols, (addr + 1,))
        if i > len(lib.symbols) or i <= 0:
            return fmtaddr(addr), addr, False
        addr, name = lib.symbols[i - 1]
        is_virtual = lib.is_virtual
        return name, addr, is_virtual

    def filter(self, profiles):
        filtered_profiles = []
        for prof in profiles:
            current = []
            for addr in prof[0]:
                name, true_addr, is_virtual = self.lookup(addr)
                if is_virtual:
                    current.append(name)
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1]))
        return filtered_profiles

    def filter_addr(self, profiles, only_virtual=True):
        filtered_profiles = []
        addr_set = set()
        for prof in profiles:
            current = []
            for addr in prof[0]:
                name, addr, is_virtual = self.lookup(addr)
                if is_virtual or not only_virtual:
                    current.append(addr)
                    addr_set.add(addr)
            if current:
                current.reverse()
                filtered_profiles.append((current, prof[1]))
        return filtered_profiles, addr_set

    def dump_stack(self, stacktrace):
        for addr in stacktrace:
            print fmtaddr(addr), self.lookup(addr)[0]


class Stats(object):
    def __init__(self, profiles, adr_dict=None):
        self.profiles = profiles
        self.adr_dict = adr_dict
        self.functions = {}
        self.generate_top()

    def generate_top(self):
        for profile in self.profiles:
            current_iter = {}
            for addr in profile[0]:
                if addr not in current_iter:  # count only topmost
                    self.functions[addr] = self.functions.get(addr, 0) + 1
                    current_iter[addr] = None

    def top_profile(self):
        return [(self._get_name(k), v) for (k, v) in self.functions.iteritems()]

    def _get_name(self, addr):
        if self.adr_dict is not None:
            return self.adr_dict[addr]
        return addr

    def function_profile(self, top_function):
        """ Show functions that we call (directly or indirectly) under
        a given addr
        """
        result = {}
        total = 0
        for profile in self.profiles:
            current_iter = {}  # don't count twice
            counting = False
            for addr in profile[0]:
                if counting:
                    if addr in current_iter:
                        continue
                    current_iter[addr] = None
                    result[addr] = result.get(addr, 0) + 1
                else:
                    if addr == top_function:
                        counting = True
                        total += 1
        result = result.items()
        result.sort(lambda a, b: cmp(a[1], b[1]))
        return result, total

    def get_tree(self):
        top_addr = self.profiles[0][0][0]
        top = Node(top_addr, self._get_name(top_addr))
        for profile in self.profiles:
            cur = top
            for i in range(1, len(profile[0])):
                addr = profile[0][i]
                cur = cur.add_child(addr, self._get_name(addr))
        return top

class Node(object):
    """ children is a dict of addr -> Node
    """
    def __init__(self, addr, name):
        self.children = {}
        self.name = name
        self.addr = addr
        self.count = 1 # starts at 1

    def add_child(self, addr, name):
        try:
            next = self.children[addr]
            next.count += 1
        except KeyError:
            next = Node(addr, name)
            self.children[addr] = next
        return next

    def __repr__(self):
        items = self.children.items()
        items.sort()
        child_str = ", ".join([("(%d, %s)" % (v.count, v.name))
                               for k, v in items])
        return '<Node: %s (%d) [%s]>' % (self.name, self.count, child_str)
