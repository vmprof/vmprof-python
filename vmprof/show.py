from __future__ import absolute_import

import os
import six
import vmprof
import argparse


class color(six.text_type):
    RED = '\033[31m'
    WHITE = '\033[37m'
    BLUE = '\033[94m'
    BOLD = '\033[1m'
    END = '\033[0m'

    def __new__(cls, content, color, bold=False):
        return six.text_type.__new__(
            cls, "%s%s%s%s" % (color, cls.BOLD if bold else "", content, cls.END))


class PrettyPrinter(object):
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
        assert(indent is None or (indent >= 0 and indent <= 4))
        self._prune_percent = prune_percent or 0.
        self._prune_level = prune_level or 1000
        self._indent = indent or 2

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

        #vmprof.cli.show(stats)
        tree = stats.get_tree()

        self._print_tree(tree)

    def _walk_tree(self, parent, node, level, callback):
        callback(parent, node, level)
        level += 1
        if level > self._prune_level:
            return
        for c in six.itervalues(node.children):
            self._walk_tree(node, c, level, callback)

    def _print_tree(self, tree):
        total = float(tree.count)

        def print_node(parent, node, level):
            parent_name = parent.name if parent else None

            perc = round(100. * float(node.count) / total, 1)
            if parent and parent.count:
                perc_of_parent = round(100. * float(node.count) / float(parent.count), 1)
            else:
                perc_of_parent = 100.

            if perc >= self._prune_percent:
                parts = node.name.count(':')

                if parts == 3:
                    block_type, funname, funline, filename = node.name.split(':')

                    p2 = color(funname, color.BLUE, bold=True)
                    p2b = color(('.' * level * self._indent), color.BLUE)

                    p3 = []
                    if os.path.dirname(filename):
                        p3.append(color(os.path.dirname(filename) + '/', color.WHITE))
                    p3.append(color(os.path.basename(filename), color.WHITE, bold=True) + ":")
                    p3.append(color("{}".format(funline), color.WHITE))
                    p3 = ''.join(p3)

                elif parts == 1:
                    block_type, funname = node.name.split(':')
                    p2 = color("JIT code", color.RED, bold=True)
                    p2b = color('.' * level * self._indent, color.RED, bold=False)
                    p3 = color(funname, color.WHITE, bold=False)

                else:
                    p2 = color(node.name, color.WHITE)
                    p2b = color(('.' * level * self._indent), color.WHITE)
                    p3 = "<unknown>"

                p1 = color("{:>5}%".format(perc), color.WHITE, bold=True)
                p4 = color("{}%".format(perc_of_parent), color.WHITE, bold=True)

                print("{} {} {}  {}  {}".format(p1, p2b, p2, p4, p3))

        self._walk_tree(None, tree, 0, print_node)


def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("profile")

    parser.add_argument(
        '--prune_percent',
        type=float,
        default=0,
        help="The indention per level within the call graph.")

    parser.add_argument(
        '--prune_level',
        type=int,
        default=None,
        help='Prune output of a profile stats node when CPU.')

    parser.add_argument(
        '--indent',
        type=int,
        default=2,
        help='The indention per level within the call graph.')

    args = parser.parse_args()

    pp = PrettyPrinter(
        prune_percent=args.prune_percent,
        prune_level=args.prune_level,
        indent=args.indent)

    pp.show(args.profile)


if __name__ == '__main__':
    main()
