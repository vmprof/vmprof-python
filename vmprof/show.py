from __future__ import absolute_import

import os
import sys
import six
import click
import vmprof


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
            stats = vmprof.read_profile(profile, virtual_only=True, include_extra_info=True)
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
                    p2 = click.style(funname, fg='blue', bold=True)
                    p2b = click.style(('.' * level * self._indent), fg='blue', bold=False)
                    p3 = []
                    if os.path.dirname(filename):
                        p3.append(click.style(os.path.dirname(filename) + '/', fg='white', bold=False))
                    p3.append(click.style(os.path.basename(filename), fg='white', bold=True) + ':')
                    p3.append(click.style("{}".format(funline), fg='white', bold=False))
                    p3 = ''.join(p3)
                    p5 = click.style("{:>2}".format(level), fg='blue', bold=False)

                elif parts == 1:
                    block_type, funname = node.name.split(':')
                    p2 = click.style("JIT code", fg='red', bold=True)
                    p2b = click.style(('.' * level * self._indent), fg='red', bold=False)
                    p3 = click.style(funname, fg='white', bold=False)
                    p5 = click.style("{:>2}".format(level), fg='red', bold=False)

                else:
                    raise Exception("fail!")

                p1 = click.style("{:>5}%".format(perc), fg='white', bold=True)
                p4 = click.style("{}%".format(perc_of_parent), fg='white', bold=True)

                print("{} {} {}  {}  {}".format(p1, p2b, p2, p4, p3))

        self._walk_tree(None, tree, 0, print_node)


@click.command()
#@click.argument('profile', type=click.File('rb')) # can't use, since vmprof can't consume a FD
@click.argument('profile', type=str)
@click.option('--prune_percent', type=float, default=0, help='The indention per level within the call graph.')
@click.option('--prune_level', type=int, default=None, help='Prune output of a profile stats node when CPU.')
@click.option('--indent', type=int, default=2, help='The indention per level within the call graph.')
def main(profile, prune_percent, prune_level, indent):
    pp = PrettyPrinter(prune_percent=prune_percent, prune_level=prune_level, indent=indent)
    pp.show(profile)


if __name__ == '__main__':
    main()
