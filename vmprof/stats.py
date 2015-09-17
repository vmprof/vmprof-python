import six
from vmprof.addrspace import JittedVirtual, JitAddr, VirtualFrame,\
     BaseMetaFrame

class EmptyProfileFile(Exception):
    pass

class Stats(object):
    def __init__(self, profiles, adr_dict=None, jit_frames=None, interp=None):
        self.profiles = profiles
        self.adr_dict = adr_dict
        self.functions = {}
        self.generate_top()
        if jit_frames is None:
            jit_frames = set()
        if interp is None:
            interp = 'pypy' # why not
        self.interp = interp
        self.jit_frames = jit_frames

    def display(self, no):
        prof = self.profiles[no][0]
        return [self._get_name(elem) for elem in prof]

    def generate_top(self):
        for profile in self.profiles:
            current_iter = {}
            for addr in profile[0]:
                if addr not in current_iter:  # count only topmost
                    self.functions[addr] = self.functions.get(addr, 0) + 1
                    current_iter[addr] = None

    def top_profile(self):
        return [(self._get_name(k), v) for (k, v) in six.iteritems(self.functions)]

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
        result = sorted(result.items(), key=lambda a: a[1])
        return result, total

    def get_top(self, profiles):
        top_addr = 0
        for prof in profiles:
            for x in prof[0]:
                if isinstance(x, VirtualFrame):
                    top_addr = x
                    break
            if top_addr:
                break
        if not top_addr:
            raise EmptyProfileFile()
        top = Node(top_addr.addr, self._get_name(top_addr))
        top.count = len(self.profiles)
        return top

    def get_meta_from_tail(self, profile, tail_start, last_virtual):
        meta = {}
        warmup = False
        for k in range(tail_start, len(profile)):
            addr = profile[k]
            if isinstance(addr, BaseMetaFrame):
                # don't count tracing twice and only count it at the bottom
                if addr.name in ('tracing', 'blackhole'):
                    if warmup:
                        continue
                    warmup = True
                addr.add_to_meta(meta)
        # count if we're jitted - if we're warming up, we don't
        # count this frame as jitted (to avoid > 100% claims)
        if isinstance(last_virtual, JittedVirtual) and not warmup:
            meta['jit'] = meta.get('jit', 0) + 1
        return meta

    def get_tree(self):
        # fine the first non-empty profile

        top = self.get_top(self.profiles)
        addr = None
        for profile in self.profiles:
            cur = top
            last_virtual = None
            last_virtual_pos = -1
            for i in range(1, len(profile[0])):
                addr = profile[0][i]
                name = self._get_name(addr)
                if not isinstance(addr, (VirtualFrame, JittedVirtual)):
                    continue
                last_virtual = addr
                last_virtual_pos = i
                cur = cur.add_child(addr.addr, name)
                if i > 1 and isinstance(profile[0][i - 1], JitAddr):
                    jit_addr = profile[0][i - 1].addr
                    cur.jitcodes[jit_addr] = cur.jitcodes.get(jit_addr, 0) + 1

            meta = self.get_meta_from_tail(profile[0], last_virtual_pos + 1,
                                           last_virtual)
            for k, v in meta.items():
                cur.meta[k] = cur.meta.get(k, 0) + v
        # get the first "interesting" node, that is after vmprof and pypy
        # mess

        return self.filter_top(top)

    def filter_top(self, top):
        first_top = top

        while True:
            if top.name.startswith('py:<module>') and 'vmprof/__main__.py' not in top.name:
                return top
            if len(top.children) > 1:
                # pick the biggest one in case of branches in vmprof
                next = None
                count = -1
                for c in top.children.values():
                    if c.count > count:
                        count = c.count
                        next = c
                top = next
            else:
                try:
                    top = top['']
                except KeyError:
                    break

        return first_top


class Node(object):
    """ children is a dict of addr -> Node
    """
    _self_count = None
    flat = False

    def __init__(self, addr, name, count=1, children=None):
        if children is None:
            children = {}
        self.children = children
        self.name = name
        assert isinstance(addr, int)
        self.addr = addr
        self.count = count # starts at 1
        self.jitcodes = {}
        self.meta = {}

    def __getitem__(self, item):
        if isinstance(item, int):
            return self.children[item]
        for v in self.children.values():
            if item in v.name:
                return v
        raise KeyError

    def as_json(self):
        import json
        return json.dumps(self._serialize())

    def _serialize(self):
        chld = [ch._serialize() for ch in six.itervalues(self.children)]
        # if we don't make str() of addr here, JS does its
        # int -> float -> int losy convertion without
        # any warning
        return [self.name, str(self.addr), self.count, self.meta, chld]

    def _rec_count(self):
        c = 1
        for x in six.itervalues(self.children):
            c += x._rec_count()
        return c

    def walk(self, callback):
        callback(self)
        for c in six.itervalues(self.children):
            c.walk(callback)

    def cumulative_meta(self, d=None):
        if d is None:
            d = {}
        for c in six.itervalues(self.children):
            c.cumulative_meta(d)
        for k, v in six.iteritems(self.meta):
            d[k] = d.get(k, 0) + v
        return d

    def _filter(self, count):
        # XXX make a copy
        for key, c in self.children.items():
            if c.count < count:
                del self.children[key]
            else:
                c._filter(count)

    def get_self_count(self):
        if self._self_count is not None:
            return self._self_count
        self._self_count = self.count
        for elem in six.itervalues(self.children):
            self._self_count -= elem.count
        return self._self_count

    self_count = property(get_self_count)

    def add_child(self, addr, name):
        try:
            next = self.children[addr]
            next.count += 1
        except KeyError:
            next = Node(addr, name)
            self.children[addr] = next
        return next

    def __eq__(self, other):
        if not isinstance(other, Node):
            return False
        return self.name == other.name and self.addr == other.addr and self.count == other.count and self.children == other.children

    def __ne__(self, other):
        return not self == other

    def __repr__(self):
        items = sorted(self.children.items())
        child_str = ", ".join([("(%d, %s)" % (v.count, v.name))
                               for k, v in items])
        return '<Node: %s (%d) [%s]>' % (self.name, self.count, child_str)
