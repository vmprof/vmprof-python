from __future__ import absolute_import

import argparse
import os
import sys
import vmprof


class FlameGraphPrinter:
    """
    The Flame Graph [1] printer for vmprof profile files.

    [1] http://www.brendangregg.com/FlameGraphs/cpuflamegraphs.html
    """

    def __init__(self, prune_level=None):
        # (Optional[float], Optional[float]) -> None
        """
        :param prune_level: Prune output of a profile stats node when the node is deeper
            than this level down the call graph from the very top.
        """
        assert prune_level is None or (0 <= prune_level <= 1000)
        self._prune_level = prune_level or 1000

    def show(self, profile):
        # (str) -> None
        """Read and display a vmprof profile file.

        :param profile: The filename of the vmprof profile file to convert.
        """
        try:
            stats = vmprof.read_profile(profile)
        except Exception as e:
            print("Fatal: could not read vmprof profile file '{}': {}".format(profile, e),
                  file=sys.stderr)
            return
        tree = stats.get_tree()
        self.print_tree(tree)

    def _walk_tree(self, parent, node, level, lines):
        if ':' in node.name:
            block_type, funcname = node.name.split(':', 2)[:2]
            if parent:
                current = parent + ';' +funcname
            else:
                current = funcname
        else:
            current = node.name

        count = node.count

        level += 1
        if level <= self._prune_level:
            for c in node.children.values():
                count -= c.count
                self._walk_tree(current, c, level, lines)

        lines.append((current, count))

    def print_tree(self, tree):
        total = float(tree.count)
        lines = []
        self._walk_tree(None, tree, 0, lines)
        lines.sort()
        for p, c in lines:
            print(p, c)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument(
        '--prune_level',
        type=int,
        default=None,
        help='Prune output of a profile stats node when CPU.')

    args = parser.parse_args()

    pp = FlameGraphPrinter(args.prune_level)
    pp.show(args.profile)


if __name__ == '__main__':
    main()
