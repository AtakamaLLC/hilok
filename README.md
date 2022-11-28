= Hierarchical locks

```
from hilok import HiLok

h = HiLok()

rd = h.read("/some/path")

# nonblocking, this will fail!
try:
    wr = h.write("/some", block=False)
except TimeoutError:
    pass

rd.release()

wr = h.write("/some")

# timeout=0 is the the same as block=False
rd = h.read("/some/path", block=False)
```
