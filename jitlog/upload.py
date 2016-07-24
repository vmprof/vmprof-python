import gzip
import tempfile

def compress_file(filename):
    fileno, name = tempfile.mkstemp(prefix='jit', suffix='.log.zip')
    os.close(fileno)
    with open(filename, 'rb') as fd:
        with gzip.open(name, 'wb') as zipfd:
            while True:
                chunk = fd.read(1024)
                if not chunk:
                    break
                zipfd.write(chunk)
    return name

def upload(filepath, url):
    zfilepath = compress_file(filepath)
    with open(zfilepath, 'rb') as fd:
        r = requests.post(url, files={ 'file': fd })
        checksum = r.text[1:-1]
        sys.stderr.write("PyPy JIT log: %s/#/%s/traces\n" % (host.rstrip("/"), checksum))

