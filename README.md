# Hierarchical locks

Allows the caller to lock a "directory", or an individual file.

Locks are, by default, shared, recursive & timed.

First optional argument to the lock manager is the "sep".

Second optional argument is "flags" (default is HiLokFlags.RECURSIVE, can be also be HiLokFlags:STRICT).

```python
from hilok import HiLok, HiLokError, HiLokFlags

h = HiLok()     # default sep is '/', can pass it in here

rd = h.read("/some/path")

# nonblocking, this will fail!
try:
    wr = h.write("/some", block=False)
except HiLokError:
    pass

rd.release()

wr = h.write("/some")

# timeout=0 is the the same as block=False
rd = h.read("/some/path", block=False)

# with syntax is fine:
with h.read("/some/path"):
    pass
```

Lock escalation (read/write/release-read) and de-escalation (write/read/release-write) are supported when RECURSIVE is enabled.
