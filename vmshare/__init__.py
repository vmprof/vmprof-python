
def get_url(host, path):
    if host.startswith("http"):
        url = '%s/%s' % (host.rstrip("/"), path)
    else:
        url = 'http://%s/%s' % (host.rstrip("/"), path)
    return url
