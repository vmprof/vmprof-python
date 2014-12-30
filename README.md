# VMprof Python client


## install

```bash
pip install vmprof
```


## use
```python
from vmprof import VMprof

vmprof = VMprof("10.0.0.1", "8000")

@vmprof
def hard_things():
	# things


hard_things()
```

## django

Inside the `wsgi` handler file (pointed by `WSGI_APPLICATION` in settings.py)

```python
import os
os.environ.setdefault("DJANGO_SETTINGS_MODULE", "server.settings")

from django.core.wsgi import get_wsgi_application
from vmprof import DjangoVMPROF

vmprof = DjangoVMPROF("localhost", 8000, "token")

app = vmprof(get_wsgi_application())
```

To profile a request, you will have to add a query string key `vmprof` with `token` value. Inside the presented snippet the token is the `token` string, as for usecase will be:

```
http://localhost:8000?vmprof=token
```
