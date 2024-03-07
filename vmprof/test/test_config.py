import tempfile

from vmprof import cli


def ini_config(content):
    _, name = tempfile.mkstemp()
    f = open(name, 'w')
    f.write(content)
    f.close()
    return name


def test_parser_config():
    test_file = ini_config("""
[global]
period = 10.0
    """)

    args = cli.parse_args([
        '--config', test_file,
        'example.py'
    ])

    assert args.period == 10.0


def test_parser_arg_precedence():
    test_file = ini_config("""
[global]
period = 10.0
web-url = example.com
    """)

    args = cli.parse_args([
        '--config', test_file,
        '--period', '5.0',
        'example.py'
    ])

    assert args.period == 5.0
    assert args.web is False
    assert args.web_url == "example.com"
    assert args.mem == False


def test_parser_without_section():

    test_file = ini_config("""
[global]
period = 0.1
web-url=example.com
enable-nonvirtual = False
no-native = True
    """)

    args = cli.parse_args([
        '--config', test_file,
        'example.py'
    ])

    assert test_file == args.config.name
    assert args.no_native == True


def test_parser_with_m_switch():
    args = cli.parse_args(['-m', 'example'])

    assert args.module
    assert args.program == 'example'
