from __future__ import print_function

import argparse
from six.moves import configparser


def build_argparser():
    parser = argparse.ArgumentParser(
        description='VMprof',
        prog="vmprof"
    )
    parser.add_argument(
        'program',
        help='program'
    )

    parser.add_argument(
        'args',
        nargs=argparse.REMAINDER,
        help='program arguments'
    )

    parser.add_argument(
        '--config',
        type=argparse.FileType('r'),
    )

    parser.add_argument(
        '--period', '-p',
        type=float,
        default=0.001,
        help='Sampling period (in microseconds)'
    )

    parser.add_argument(
        '--web-auth',
        help='Authtoken for your acount on the server, works only when --web is used'
    )

    parser.add_argument(
        '--web-url',
        metavar='url',
        default='http://vmprof.com',
        help='Provide URL instead of the default vmprof.com)'
    )
    parser.add_argument(
        '--mem',
        action="store_true",
        help='Do memory profiling as well',
    )
    parser.add_argument(
        '--lines',
        action="store_true",
        help='Store lines execution stats',
    )
    parser.add_argument(
        '--jitlog',
        action='store_true',
        help='Upload the jitlog to remote server (defaults to vmprof.com)',
    )
    output_mode_args = parser.add_mutually_exclusive_group()
    output_mode_args.add_argument(
        '--web',
        action='store_true',
        help='Upload profiling stats to a remote server (defaults to vmprof.com)'
    )
    output_mode_args.add_argument(
        '--output', '-o',
        metavar='file.prof',
        type=argparse.FileType('w+b'),
        help='Save profiling data to file'
    )

    return parser


def parse_args(argv):
    parser = build_argparser()
    args = parser.parse_args(argv)
    if args.config:
        ini_options = [
            ('period', float),
            ('web', str),
            ('web-auth', str),
            ('web-url', str),
            ('output', str)
        ]

        ini_parser = IniParser(args.config)

        for name, type in ini_options:
            argname = name.replace("-", "_")
            default = parser.get_default(argname)
            current = getattr(args, argname, default)
            if current == default:
                value = ini_parser.get_option(name, type, default)
                setattr(args, argname, value)

    return args


class IniParser(object):

    def __init__(self, f):
        self.ini_parser = configparser.ConfigParser()
        self.ini_parser.readfp(f)

    def get_option(self, name, type, default=None):
        if type == float:
            try:
                return self.ini_parser.getfloat('global', name)
            except configparser.NoOptionError:
                return default
        elif type == bool:
            try:
                return self.ini_parser.getboolean('global', name)
            except configparser.NoOptionError:
                return default

        try:
            return self.ini_parser.get('global', name)
        except configparser.NoOptionError:
            return default


def show(stats):
    p = stats.top_profile()
    if not p:
        print("no stats")
        return

    p.sort(key=lambda x: x[1], reverse=True)
    top = p[0][1]

    max_len = max([_namelen(e[0]) for e in p])

    print(" vmprof output:")
    print(" %:      name:" + " " * (max_len - 3) + "location:")

    for k, v in p:
        v = "%.1f%%" % (float(v) / top * 100)
        if v == '0.0%':
            v = '<0.1%'
        if k.startswith('py:'):
            try:
                _, func_name, lineno, filename = k.split(":", 3)
                lineno = int(lineno)
            except ValueError:
                print(" %s %s" % (v.ljust(7), k.ljust(max_len + 1)))
                # badly done split
            else:
                print(" %s %s %s:%d" % (v.ljust(7), func_name.ljust(max_len + 1), filename, lineno))
        else:
            print(" %s %s" % (v.ljust(7), k.ljust(max_len + 1)))


def _namelen(e):
    if e.startswith('py:'):
        return len(e.split(':')[1])
    else:
        return len(e)
