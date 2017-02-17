import platform
import argparse
from vmshare.service import Service

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument("--web-url", default='http://vmprof.com', help='Target host')
    parser.add_argument("--web-auth", default=None, help='Authtoken for your acount on the server')
    args = parser.parse_args()

    host, auth = args.web_url, args.web_auth
    filename = args.profile
    service = Service(host, auth)
    interpname = platform.python_implementation()
    service.post({ Service.FILE_CPU_PROFILE: filename,
                   Service.FILE_JIT_PROFILE: filename + '.jit',
                   'argv': interpname + ' -m vmprof.upload ' + filename,
                   'VM': interpname })

if __name__ == '__main__':
    main()
