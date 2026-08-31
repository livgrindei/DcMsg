import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from dcmsg import DcMsg


class ScalarRoundTrip(unittest.TestCase):
    def test_roundtrip(self):
        m = DcMsg()
        m.AddBool("b", True)
        m.AddUByte("ub", 200)
        m.AddByte("sb", -100)
        m.AddUInt("ui", 4000000000)
        m.AddInt("si", -2000000000)
        m.AddULong("ul", 18000000000000000000)
        m.AddLong("sl", -9000000000000000000)
        m.AddFloat("f", 3.5)
        m.AddDouble("d", -2.718281828)
        m.AddString("s", "hello world")

        r = DcMsg(m.GetData())
        d = r.GetDictionary()
        self.assertEqual(bool(d["b"]), True)
        self.assertEqual(int(d["ub"]), 200)
        self.assertEqual(int(d["sb"]), -100)
        self.assertEqual(int(d["ui"]), 4000000000)
        self.assertEqual(int(d["si"]), -2000000000)
        self.assertEqual(int(d["ul"]), 18000000000000000000)
        self.assertEqual(int(d["sl"]), -9000000000000000000)
        self.assertAlmostEqual(float(d["f"]), 3.5, places=5)
        self.assertAlmostEqual(float(d["d"]), -2.718281828, places=9)
        self.assertEqual(d["s"], "hello world")


class MemoryArray(unittest.TestCase):
    def test_roundtrip(self):
        m = DcMsg()
        m.AddMemoryArray("mem", bytes([1, 2, 3, 4, 5]))
        r = DcMsg(m.GetData())
        got = r.GetDictionary()["mem"]
        self.assertTrue(np.array_equal(got, np.array([1, 2, 3, 4, 5], dtype=np.uint8)))


class FixedArrays(unittest.TestCase):
    def test_roundtrip(self):
        m = DcMsg()
        m.AddBoolArray("bools", [True, False, True])
        m.AddUByteArray("ubytes", [10, 20, 30, 255])
        m.AddByteArray("bytes", [-10, 0, 10, 127])
        m.AddUIntArray("uints", [1, 2, 4000000000])
        m.AddIntArray("ints", [-1, 0, 2000000000])
        m.AddULongArray("ulongs", [1, 18000000000000000000])
        m.AddLongArray("longs", [-9000000000000000000, 42])
        m.AddFloatArray("floats", [1.5, -2.25])
        m.AddDoubleArray("doubles", [1.123456789, -9.87654321])
        m.AddStringArray("strs", ["alpha", "beta", "gamma!"])

        r = DcMsg(m.GetData())
        d = r.GetDictionary()
        self.assertEqual(list(d["bools"]), [True, False, True])
        self.assertTrue(np.array_equal(d["ubytes"], np.array([10, 20, 30, 255], dtype=np.uint8)))
        self.assertTrue(np.array_equal(d["bytes"], np.array([-10, 0, 10, 127], dtype=np.int8)))
        self.assertTrue(np.array_equal(d["uints"], np.array([1, 2, 4000000000], dtype=np.uint32)))
        self.assertTrue(np.array_equal(d["ints"], np.array([-1, 0, 2000000000], dtype=np.int32)))
        self.assertTrue(np.array_equal(d["ulongs"], np.array([1, 18000000000000000000], dtype=np.uint64)))
        self.assertTrue(np.array_equal(d["longs"], np.array([-9000000000000000000, 42], dtype=np.int64)))
        self.assertTrue(np.allclose(d["floats"], [1.5, -2.25]))
        self.assertTrue(np.allclose(d["doubles"], [1.123456789, -9.87654321]))
        self.assertEqual(list(d["strs"]), ["alpha", "beta", "gamma!"])


class NestedMessage(unittest.TestCase):
    def test_message(self):
        sub = DcMsg()
        sub.AddInt("inner", 7)
        sub.AddString("name", "child")

        m = DcMsg()
        m.AddMessage("sub", sub)

        r = DcMsg(m.GetData())
        got_sub = r.GetDictionary()["sub"]
        self.assertEqual(int(got_sub.GetDictionary()["inner"]), 7)
        self.assertEqual(got_sub.GetDictionary()["name"], "child")

    def test_message_array(self):
        s1 = DcMsg()
        s1.AddInt("x", 1)
        s2 = DcMsg()
        s2.AddInt("x", 2)

        m = DcMsg()
        m.AddMessageArray("arr", [s1, s2])

        r = DcMsg(m.GetData())
        got_arr = r.GetDictionary()["arr"]
        self.assertEqual(len(got_arr), 2)
        self.assertEqual(int(got_arr[0].GetDictionary()["x"]), 1)
        self.assertEqual(int(got_arr[1].GetDictionary()["x"]), 2)


class ValidationAndErrors(unittest.TestCase):
    def test_duplicate_name_rejected(self):
        m = DcMsg()
        self.assertTrue(m.AddInt("dup", 1))
        self.assertFalse(m.AddInt("dup", 2))

    def test_read_only_rejects_add(self):
        m = DcMsg()
        m.AddInt("x", 1)
        r = DcMsg(m.GetData())
        self.assertFalse(r.AddInt("y", 2))

    def test_wrong_type_argument_raises(self):
        m = DcMsg()
        with self.assertRaises(ValueError):
            m.AddValue("bad", object())

    def test_empty_message_array_rejected(self):
        m = DcMsg()
        self.assertFalse(m.AddMessageArray("empty", []))

    def test_truncated_buffer_raises(self):
        with self.assertRaises(Exception):
            DcMsg(b"short")


if __name__ == "__main__":
    unittest.main()
