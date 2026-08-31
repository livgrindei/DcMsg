"""Cross-language wire-compatibility check between the C++ and Python DcMsg
implementations.

Usage: test_cross_lang.py <path-to-cross_lang_helper-binary>

Direction 1 (C++ -> Python): runs the helper in "encode" mode, decodes the
resulting buffer with the Python port, and checks every field.

Direction 2 (Python -> C++): builds an equivalent message with the Python
port, writes it to a file, and runs the helper in "decode" mode against it
(the helper does its own field-by-field verification and reports via its
exit code / OK-FAIL output).
"""

import os
import subprocess
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from dcmsg import DcMsg


def cpp_to_python(helper: str) -> bool:
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        path = tmp.name
    try:
        rc = subprocess.call([helper, "encode", path])
        if rc != 0:
            print("FAIL: helper encode failed")
            return False

        with open(path, "rb") as f:
            buf = f.read()

        m = DcMsg(buf)
        d = m.GetDictionary()

        ok = True
        checks = [
            ("b", True, bool(d["b"])),
            ("ui", 4000000000, int(d["ui"])),
            ("s", "hello from cpp", d["s"]),
            ("msg.inner", 77, int(d["msg"].GetDictionary()["inner"])),
            ("msg.name", "child", d["msg"].GetDictionary()["name"]),
            ("strs", ["alpha", "beta", "gamma!"], list(d["strs"])),
            ("msgs[0].x", 1, int(d["msgs"][0].GetDictionary()["x"])),
            ("msgs[1].x", 2, int(d["msgs"][1].GetDictionary()["x"])),
        ]
        for name, expected, actual in checks:
            if expected != actual:
                print(f"FAIL: {name}: expected={expected!r} actual={actual!r}")
                ok = False

        if not np.array_equal(d["mem"], np.array([1, 2, 3, 4, 5], dtype=np.uint8)):
            print("FAIL: mem")
            ok = False
        if not np.array_equal(d["uints"], np.array([1, 2, 4000000000], dtype=np.uint32)):
            print("FAIL: uints")
            ok = False

        if ok:
            print("cpp -> python: OK")
        return ok
    finally:
        os.unlink(path)


def python_to_cpp(helper: str) -> bool:
    sub = DcMsg()
    sub.AddInt("inner", 99)
    sub.AddString("name", "pychild")

    m = DcMsg()
    m.AddBool("b", False)
    m.AddUInt("ui", 3000000000)
    m.AddDouble("d", 6.02214076e23)
    m.AddString("s", "hello from python")
    m.AddMessage("msg", sub)
    m.AddMemoryArray("mem", bytes([9, 8, 7, 6]))
    m.AddUIntArray("uints", [100, 200, 300])
    m.AddStringArray("strs", ["one", "two", "three"])

    s1 = DcMsg()
    s1.AddInt("y", 10)
    s2 = DcMsg()
    s2.AddInt("y", 20)
    m.AddMessageArray("msgs", [s1, s2])

    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        path = tmp.name
    try:
        with open(path, "wb") as f:
            f.write(m.GetData())

        result = subprocess.run([helper, "decode", path], capture_output=True, text=True)
        print(result.stdout, end="")
        if result.returncode != 0:
            print("FAIL: helper decode failed")
            return False
        print("python -> cpp: OK")
        return True
    finally:
        os.unlink(path)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path-to-cross_lang_helper>")
        return 2

    helper = sys.argv[1]
    ok = cpp_to_python(helper)
    ok = python_to_cpp(helper) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
