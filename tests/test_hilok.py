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


def test_early_rel():
    h = HiLok(recursive=False)
    with h.write("/a/b") as l:
        l.release()
        h.write("/a/b", block=False)


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
