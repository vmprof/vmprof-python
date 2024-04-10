
import inspect
import linecache
import os
import sys
import tokenize
import vmprof
import argparse
from collections import namedtuple

from vmprof.stats import EmptyProfileFile


class color(str):
    RED = '\033[31m'
    WHITE = '\033[37m'
    BLUE = '\033[94m'
    BOLD = '\033[1m'
    END = '\033[0m'

    def __new__(cls, content, color, bold=False):
        return "%s%s%s%s" % (color, cls.BOLD if bold else "", content, cls.END)

class AbstractPrinter:
    def show(self, profile):
        """
        Read and display a vmprof profile file.

        :param profile: The filename of the vmprof profile file to display.
        :type profile: str
        """
        try:
            stats = vmprof.read_profile(profile)
        except Exception as e:
            print("Fatal: could not read vmprof profile file '{}': {}".format(profile, e))
            return

        if stats.get_runtime_in_microseconds() < 1000000:
            msg = color("WARNING: The profiling completed in less than 1 seconds. Please run your programs longer!\r\n", color.RED)
            sys.stderr.write(msg)

        try:
            tree = stats.get_tree()
            self._show(tree)
        except EmptyProfileFile as e:
            print("No stack trace has been recorded (profile is empty)!")


class PrettyPrinter(AbstractPrinter):
    """
    A pretty print for vmprof profile files.
    """

    def __init__(self, prune_percent=None, prune_level=None, indent=None):
        """

        :param prune_percent: Prune output of a profile stats node when CPU consumed
            is less than this value for the node and everything below.
        :type prune_percent: None or float
        :param prune_level: Prune output of a profile stats node when the node is deeper
            than this level down the call graph from the very top.
        :type prune_level: None or int
        :param indent: The indention per level within the call graph.
        :type indent: None or int
        """
        assert(prune_percent is None or (prune_percent >= 0 and prune_percent <= 100))
        assert(prune_level is None or (prune_level >= 0 and prune_level <= 1000))
        assert(indent is None or (indent >= 0 and indent <= 8))
        self._prune_percent = prune_percent or 0.
        self._prune_level = prune_level or 1000
        self._indent = indent or 2

    def _show(self, tree):
        self._print_tree(tree)

    def _walk_tree(self, parent, node, level, callback):
        callback(parent, node, level)
        level += 1
        if level > self._prune_level:
            return
        for c in node.children.values():
            self._walk_tree(node, c, level, callback)

    color = color

    def _print_node(self, parent, node, level, total):
        color = self.color
        perc = round(100. * float(node.count) / total, 1)
        if parent and parent.count:
            perc_of_parent = round(100. * float(node.count) / float(parent.count), 1)
        else:
            perc_of_parent = 100.

        if self._indent:
            level_indent = "|" + "." * (self._indent-1)
        else:
            level_indent = ""

        if perc >= self._prune_percent:
            parts = node.name.count(':')

            if parts == 3:
                block_type, funname, funline, filename = node.name.split(':')

                p2 = color(funname, color.BLUE, bold=True)
                indent = color(level_indent * level, color.BLUE)

                p3 = []
                if os.path.dirname(filename):
                    p3.append(color(os.path.dirname(filename) + '/', color.WHITE))
                p3.append(color(os.path.basename(filename), color.WHITE, bold=True) + ":")
                p3.append(color("{}".format(funline), color.WHITE))
                p3 = ''.join(p3)

            elif parts == 1:
                block_type, funname = node.name.split(':')
                p2 = color("JIT code", color.RED, bold=True)
                indent = color(level_indent * level, color.RED, bold=False)
                p3 = color(funname, color.WHITE, bold=False)
            else:
                p2 = color(node.name, color.WHITE)
                indent = color(level_indent * level, color.WHITE)
                p3 = color("<unknown>", color.WHITE)

            p1 = color("{:>5}%".format(perc), color.WHITE, bold=True)
            p4 = color("{}%".format(perc_of_parent), color.WHITE, bold=True)

            self._print_line(p1, indent, "{}  {}  {}".format(p2, p4, p3))

    def _print_line(self, leader, indent, ln):
        print("{} {} {}".format(leader, indent, ln))

    def _print_tree(self, tree):
        from functools import partial
        self._walk_tree(
                None, tree, 0,
                partial(self._print_node, total=float(tree.count)))


class html_color(str):
    RED = 'red'
    WHITE = 'black'
    BLUE = 'blue'
    BOLD = 'bold'

    def __new__(cls, content, color, bold=False):
        from html import escape
        content = escape(content)
        if bold:
            content = "<b>{}</b>".format(content)
        if color:
            content = "<span style='color: {}'>{}</span>".format(color, content)
        return content


class HTMLPrettyPrinter(PrettyPrinter):
    def _show(self, tree):
        print("<!doctype html>")
        print("<body>")
        print("<style>")
        print("  details { margin-left: 3em; }")
        print("</style>")
        self._print_tree(tree)
        print("</body>")

    def _walk_tree(self, parent, node, level, callback):
        print("<details>")
        callback(parent, node, level)
        level += 1
        if level > self._prune_level:
            return
        for c in node.children.values():
            self._walk_tree(node, c, level, callback)
        print("</details>")

    def _print_line(self, leader, indent, ln):
        print("<summary><tt>{} {}</tt><br></summary>".format(leader, ln))

    color = html_color


NodeDescr = namedtuple(
        'NodeDescr',
        ['block_type', 'funname', 'funline', 'filename'])


def parse_block_name(node_name):
    nparts = node_name.count(':')+1

    block_type = None
    funline = None
    filename = None
    if nparts == 4:
        block_type, funname, funline, filename = node_name.split(':')
    elif nparts == 2:
        block_type, funname = node_name.split(':')
    else:
        funname = node_name

    return NodeDescr(block_type, funname, funline, filename)


class FlatPrinter(AbstractPrinter):
    """
    A per-function pretty-printer of vmprof profiles.
    """

    def __init__(self, include_callees, no_native, percent_cutoff):
        self.include_callees = include_callees
        self.no_native = no_native
        self.percent_cutoff = percent_cutoff

    def _show(self, tree):
        self._print_tree(tree)

    def _walk_tree(self, parent, node, callback):
        callback(parent, node)
        for c in node.children.values():
            self._walk_tree(node, c, callback)

    def _print_tree(self, tree):
        func_id_to_count = {}

        def collect_node(parent, node):
            ndescr = parse_block_name(node.name)
            if self.no_native and ndescr.block_type == 'n':
                return

            mycount = node.count
            if not self.include_callees:
                mycount = mycount - sum(
                        0 if parse_block_name(ch.name)[0] == 'n' and self.no_native
                        else ch.count

                    for ch in node.children.values())

            func_id_to_count[ndescr] = func_id_to_count.get(ndescr, 0) + mycount

        self._walk_tree(None, tree, collect_node)

        cost_list = sorted(
                func_id_to_count.items(),
                key=(lambda item: item[1]),
                reverse=True)

        total = float(tree.count)

        for ndescr, count in cost_list:
            percent = count/total*100

            if percent >= self.percent_cutoff:
                print(
                        '{percent:10.3f}% - {funcname}:{filename}:{funline}'
                        .format(
                            percent=percent,
                            funcname=color(ndescr.funname, color.WHITE, bold=True),
                            filename=ndescr.filename,
                            funline=ndescr.funline))


class LinesPrinter(AbstractPrinter):
    def __init__(self, filter=None):
        self.filter = filter

    def _show(self, tree):
        for (filename, funline, funname), line_stats in self.lines_stat(tree):
            if self.filter is None or self.filter in funname or \
                                      self.filter in filename:
                self.show_func(filename, funline, funname, line_stats)

    def lines_stat(self, tree):
        funcs = {}

        def walk(node, d):
            if node is None:
                return

            parts = node.name.count(':')
            if parts == 3:
                block_type, funname, funline, filename = node.name.split(':')
                # only python supported for line profiling
                if block_type == 'py':
                    lines = d.setdefault((filename, int(funline), funname), {})
                    for l, cnt in node.lines.items():
                        lines[l] = lines.get(l, 0) + cnt

            for c in node.children.values():
                walk(c, d)

        walk(tree, funcs)

        return funcs.items()

    def show_func(self, filename, start_lineno, func_name, timings, stream=None, stripzeros=False):
        """ Show results for a single function.
        """
        if stream is None:
            stream = sys.stdout

        template = '%6s %8s %8s  %-s'
        d = {}
        total_hits = 0.0

        linenos = []
        for lineno, nhits in timings.items():
            total_hits += nhits
            linenos.append(lineno)

        if stripzeros and total_hits == 0:
            return

        stream.write("Total hits: %g s\n" % total_hits)
        if os.path.exists(filename) or filename.startswith("<ipython-input-"):
            stream.write("File: %s\n" % filename)
            stream.write("Function: %s at line %s\n" % (func_name, start_lineno))
            if os.path.exists(filename):
                # Clear the cache to ensure that we get up-to-date results.
                linecache.clearcache()
            all_lines = linecache.getlines(filename)
            try:
                sublines = inspect.getblock(all_lines[start_lineno-1:])
            except tokenize.TokenError:
                # the problem stems from getblock not being able to tokenize such an example:
                # >>> inspect.getblock(['i:i**i','}']) => TokenError
                # e.g. it fails on multi line dictionary comprehensions
                # the current approach is best effort, but cuts of some lines at the top.
                # see issue #118 for details
                sublines = all_lines[start_lineno-1:max(linenos)]
        else:
            stream.write("\n")
            stream.write("Could not find file %s\n" % filename)
            stream.write("Are you sure you are running this program from the same directory\n")
            stream.write("that you ran the profiler from?\n")
            stream.write("Continuing without the function's contents.\n")
            # Fake empty lines so we can see the timings, if not the code.
            nlines = max(linenos) - min(min(linenos), start_lineno) + 1
            sublines = [''] * nlines
        for lineno, nhits in timings.items():
            d[lineno] = (nhits, '%5.1f' % (100* nhits / total_hits))
        linenos = range(start_lineno, start_lineno + len(sublines))
        empty = ('', '')
        header = template % ('Line #', 'Hits', '% Hits',
            'Line Contents')
        stream.write("\n")
        stream.write(header)
        stream.write("\n")
        stream.write('=' * len(header))
        stream.write("\n")
        for lineno, line in zip(linenos, sublines):
            nhits, percent = d.get(lineno, empty)
            txt = template % (lineno, nhits, percent,
                              line.rstrip('\n').rstrip('\r'))
            stream.write(txt)
            stream.write("\n")
        stream.write("\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    subp = parser.add_subparsers()

    parser_tree = subp.add_parser("tree")
    parser_tree.add_argument(
        '--html',
        action="store_true",
        help='Output the tree as interactive HTML.')
    parser_tree.add_argument(
        '--prune_percent',
        type=float,
        default=0,
        help='Prune output of a profile stats node below specified CPU samples.')
    parser_tree.add_argument(
        '--prune_level',
        type=int,
        default=None,
        help='Prune output of a profile stats node below specified depth.')
    parser_tree.add_argument(
        '--indent',
        type=int,
        default=2,
        help='The indention per level within the call graph.')
    parser_tree.set_defaults(mode='tree')

    parser_lines = subp.add_parser("lines")
    parser_lines.add_argument('--filter', dest='filter', type=str,
                        default=None, help="Filters the console output when "
                        "vmprofshow is invoked with --lines. Filters by "
                        "function names or filenames.")
    parser_lines.set_defaults(mode='lines')

    parser_flat = subp.add_parser("flat")
    parser_flat.add_argument('--include-callees', action="store_true")
    parser_flat.add_argument('--no-native', action="store_true")
    parser_flat.add_argument('--percent-cutoff', type=float, default=0)
    parser_flat.set_defaults(mode='flat')

    args = parser.parse_args()

    mode = getattr(args, 'mode', None)
    if mode is None:
        parser. print_usage()
        import sys
        sys.exit(1)

    if mode == 'lines':
        pp = LinesPrinter(filter=args.filter)
    elif mode == 'flat':
        pp = FlatPrinter(
                include_callees=args.include_callees,
                no_native=args.no_native,
                percent_cutoff=args.percent_cutoff)
    elif mode == 'tree':
        if args.html:
            cls = HTMLPrettyPrinter
        else:
            cls = PrettyPrinter
        pp = cls(
            prune_percent=args.prune_percent,
            prune_level=args.prune_level,
            indent=args.indent)
    else:
        raise ValueError("invalid value for 'mode'")

    pp.show(args.profile)


if __name__ == '__main__':
    main()
