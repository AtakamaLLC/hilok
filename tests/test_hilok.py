import pytest
from hilok import HiLok, HiLokError


def test_wr_no_lev():
    h = HiLok(recursive=False)
    lk = h.write("/a/b")
    lk.release()
    lk = h.write("/a/b")

    with pytest.raises(HiLokError):
        lk = h.write("/a/b", block=False)


def test_with_wr():
    h = HiLok(recursive=False)
    with h.write("/a/b"):
        with pytest.raises(HiLokError):
            h.write("/a/b", block=False)

    with h.write("/a/b", block=False) as hh:
        pass


def test_write_parent():
    h = HiLok(recursive=False)
    with h.read("/a/b/c/d/e"):
        with pytest.raises(HiLokError):
            h.write("/a/b", timeout=0.1)


def test_early_rel():
    h = HiLok(recursive=False)
    with h.write("/a/b") as l:
        l.release()
        h.write("/a/b", block=False)


def test_rename_norec_write():
    h = HiLok(recursive=False)
    with h.write("/a/b"):
        h.rename("/a/b", "x", block=False)
        with pytest.raises(HiLokError):
            h.write("x", block=False)
        with h.write("/a/b", block=False):
            pass
        h.rename("x", "c:/long/path/windows/style", block=False)
        h.rename("c:/long/path/windows/style", "c:/long/path/super", block=False)
        # long path super is a write lock
        with h.read("c:/long/path", block=False):
            pass
        with pytest.raises(HiLokError):
            h.write("c:/long/path", block=False)

    with pytest.raises(HiLokError):
        h.rename("notthere", "whatever")

def test_rename_norec_read():
    # real scenario from cvfs
    h = HiLok(recursive=False)
    l1 = h.read("/a/b/c/d/e/f/g")
    h.rename("/a/b/c/d/e/f/g", "/a/b/x")


def test_rename_rec_read():
    # real scenario from cvfs
    h = HiLok(recursive=True)
    l1 = h.read("/a/b/c/d/e/f/g")
    h.rename("/a/b/c/d/e/f/g", "/a/b/x")


def test_riaa():
    h = HiLok(recursive=False)
    l = h.write("/a/b")
    del l
    l = h.write("/a/b")
    del l


def test_none():
    h = HiLok(recursive=False)
    l = h.write("/a/b")
    with pytest.raises(HiLokError):
        # none is allowed, and is ignored
        l = h.write("/a/b", block=False, timeout=None)

    with pytest.raises(HiLokError):
        # none is allowed, and is ignored
        l = h.read("/a/b", block=False, timeout=None)


def test_with_rd():
    h = HiLok(recursive=False)
    with h.read("/a/b"):
        with h.read("/a/b", block=False):
            pass
        with pytest.raises(HiLokError):
            h.write("/a/b", block=False)
    with h.write("/a/b", block=False):
        pass


def test_other_sep():
    h = HiLok(":", recursive=False)
    with h.read("a:b"):
        with pytest.raises(HiLokError):
            h.write("a", block=False)
