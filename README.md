# VMprof Python client


```
pip install vmprof
```


```
from vmprof import VMprof

vmprof = VMprof("10.0.0.1", "8000")

@vmprof
def hard_things():
	# things


hard_things()
```

