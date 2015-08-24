import tempfile


def test_travis_1():
    tmpfile = tempfile.NamedTemporaryFile(dir=".")
    assert tmpfile.name
