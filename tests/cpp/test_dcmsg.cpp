// Self-contained test suite for DS::DcMsg. No external test framework —
// this keeps the library's own "no dependencies" promise extending to its
// tests. Each TEST(...) function runs independently; CHECK(...) records a
// failure but keeps running so one broken assertion doesn't hide the rest.

#include <dcmsg/dcmsg.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        g_checks++;                                                          \
        if (!(cond))                                                         \
        {                                                                    \
            g_failures++;                                                    \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                    \
    } while (0)

#define TEST(name) static void name()
#define RUN(name)                                                             \
    do                                                                        \
    {                                                                         \
        printf("-- %s --\n", #name);                                         \
        name();                                                               \
    } while (0)

using DS::DcMsg;

TEST(ScalarRoundTrip)
{
    DcMsg m;
    CHECK(m.AddBool("b", true));
    CHECK(m.AddUByte("ub", 200));
    CHECK(m.AddByte("sb", -100));
    CHECK(m.AddUShort("us", 60000));
    CHECK(m.AddShort("ss", -30000));
    CHECK(m.AddUInt("ui", 4000000000u));
    CHECK(m.AddInt("si", -2000000000));
    CHECK(m.AddULong("ul", 18000000000000000000ULL));
    CHECK(m.AddLong("sl", -9000000000000000000LL));
    CHECK(m.AddFloat("f", 3.5f));
    CHECK(m.AddDouble("d", -2.718281828));
    CHECK(m.AddString("s", std::string("hello world")));

    uint64_t size = 0;
    void* buf = m.GetData(size);
    DcMsg r(buf, size);
    CHECK(r.IsValid());

    bool b; uint8_t ub; int8_t sb; uint16_t us; int16_t ss;
    uint32_t ui; int32_t si; uint64_t ul; int64_t sl; float f; double d;
    std::string s;
    CHECK(r.GetBool("b", b) && b == true);
    CHECK(r.GetUByte("ub", ub) && ub == 200);
    CHECK(r.GetByte("sb", sb) && sb == -100);
    CHECK(r.GetUShort("us", us) && us == 60000);
    CHECK(r.GetShort("ss", ss) && ss == -30000);
    CHECK(r.GetUInt("ui", ui) && ui == 4000000000u);
    CHECK(r.GetInt("si", si) && si == -2000000000);
    CHECK(r.GetULong("ul", ul) && ul == 18000000000000000000ULL);
    CHECK(r.GetLong("sl", sl) && sl == -9000000000000000000LL);
    CHECK(r.GetFloat("f", f) && f == 3.5f);
    CHECK(r.GetDouble("d", d) && d == -2.718281828);
    CHECK(r.GetString("s", s) && s == "hello world");
}

TEST(GetWorksOnWritableInstanceDirectly)
{
    // Regression test: Get* must work on a DcMsg that was never
    // serialized/re-parsed, not only on one reconstructed via GetData().
    DcMsg m;
    m.AddInt("x", 42);
    int32_t x = 0;
    CHECK(m.GetInt("x", x) && x == 42);
}

TEST(MemoryArrayRoundTrip)
{
    DcMsg m;
    unsigned char raw[] = {1, 2, 3, 4, 5};
    CHECK(m.AddMemoryArray("mem", raw, sizeof(raw)));

    uint64_t size = 0;
    void* buf = m.GetData(size);
    DcMsg r(buf, size);
    CHECK(r.IsValid());

    void* out = nullptr;
    uint32_t out_size = 0;
    CHECK(r.GetMemoryArray("mem", &out, out_size));
    CHECK(out_size == sizeof(raw));
    CHECK(out != nullptr && memcmp(out, raw, sizeof(raw)) == 0);
    free(out);
}

TEST(FixedArraysRoundTrip)
{
    DcMsg m;
    std::vector<bool> bools = {true, false, true};
    std::vector<uint8_t> ubytes = {10, 20, 30, 255};
    std::vector<int8_t> bytes = {-10, 0, 10, 127};
    std::vector<uint32_t> uints = {1, 2, 4000000000u};
    std::vector<int32_t> ints = {-1, 0, 2000000000};
    std::vector<uint64_t> ulongs = {1, 18000000000000000000ULL};
    std::vector<int64_t> longs = {-9000000000000000000LL, 42};
    std::vector<float> floats = {1.5f, -2.25f};
    std::vector<double> doubles = {1.123456789, -9.87654321};
    std::vector<std::string> strs = {"alpha", "beta", "gamma!"};

    CHECK(m.AddBoolArray("bools", bools));
    CHECK(m.AddUByteArray("ubytes", ubytes));
    CHECK(m.AddByteArray("bytes", bytes));
    CHECK(m.AddUIntArray("uints", uints));
    CHECK(m.AddIntArray("ints", ints));
    CHECK(m.AddULongArray("ulongs", ulongs));
    CHECK(m.AddLongArray("longs", longs));
    CHECK(m.AddFloatArray("floats", floats));
    CHECK(m.AddDoubleArray("doubles", doubles));
    CHECK(m.AddStringArray("strs", strs));

    uint64_t size = 0;
    void* buf = m.GetData(size);
    DcMsg r(buf, size);
    CHECK(r.IsValid());

    std::vector<bool> bools2;
    std::vector<uint8_t> ubytes2;
    std::vector<int8_t> bytes2;
    std::vector<uint32_t> uints2;
    std::vector<int32_t> ints2;
    std::vector<uint64_t> ulongs2;
    std::vector<int64_t> longs2;
    std::vector<float> floats2;
    std::vector<double> doubles2;
    std::vector<std::string> strs2;

    CHECK(r.GetBoolArray("bools", bools2) && bools2 == bools);
    CHECK(r.GetUByteArray("ubytes", ubytes2) && ubytes2 == ubytes);
    CHECK(r.GetByteArray("bytes", bytes2) && bytes2 == bytes);
    CHECK(r.GetUIntArray("uints", uints2) && uints2 == uints);
    CHECK(r.GetIntArray("ints", ints2) && ints2 == ints);
    CHECK(r.GetULongArray("ulongs", ulongs2) && ulongs2 == ulongs);
    CHECK(r.GetLongArray("longs", longs2) && longs2 == longs);
    CHECK(r.GetFloatArray("floats", floats2) && floats2 == floats);
    CHECK(r.GetDoubleArray("doubles", doubles2) && doubles2 == doubles);
    CHECK(r.GetStringArray("strs", strs2) && strs2 == strs);
}

TEST(NestedMessageAndMessageArray)
{
    DcMsg sub;
    sub.AddInt("inner", 7);
    sub.AddString("name", "child");

    DcMsg m;
    CHECK(m.AddMessage("sub", sub));

    DcMsg s1, s2;
    s1.AddInt("x", 1);
    s2.AddInt("x", 2);
    std::vector<DcMsg> arr = {s1, s2};
    CHECK(m.AddMessageArray("arr", arr));

    uint64_t size = 0;
    void* buf = m.GetData(size);
    DcMsg r(buf, size);
    CHECK(r.IsValid());

    DcMsg got_sub;
    CHECK(r.GetMessage("sub", got_sub));
    int32_t inner;
    std::string name;
    CHECK(got_sub.GetInt("inner", inner) && inner == 7);
    CHECK(got_sub.GetString("name", name) && name == "child");

    std::vector<DcMsg> got_arr;
    CHECK(r.GetMessageArray("arr", got_arr));
    CHECK(got_arr.size() == 2);
    if (got_arr.size() == 2)
    {
        int32_t x0, x1;
        CHECK(got_arr[0].GetInt("x", x0) && x0 == 1);
        CHECK(got_arr[1].GetInt("x", x1) && x1 == 2);
    }
}

TEST(UpdateInPlace)
{
    DcMsg m;
    m.AddUInt("Channel", 1);
    m.AddDouble("Value", 101.15);

    CHECK(m.UpdateUInt("Channel", 42));
    CHECK(m.UpdateDouble("Value", 3.14));
    CHECK(!m.UpdateInt("Channel", 5));   // wrong type
    CHECK(!m.UpdateUInt("Missing", 5));  // not found

    uint32_t chn; double val;
    CHECK(m.GetUInt("Channel", chn) && chn == 42);
    CHECK(m.GetDouble("Value", val) && val == 3.14);

    uint64_t size = 0;
    void* buf = m.GetData(size);
    DcMsg r(buf, size);
    CHECK(r.IsValid());
    CHECK(!r.UpdateUInt("Channel", 99));   // read-only rejects update
}

TEST(CloneReadOnlyToEditable)
{
    DcMsg m;
    m.AddUInt("Channel", 3);
    m.AddString("Label", "sensor-3");

    uint64_t size = 0;
    void* buf = m.GetData(size);
    DcMsg readOnly(buf, size);
    CHECK(readOnly.IsValid());

    // Read-only rejects mutation directly
    CHECK(!readOnly.AddInt("New", 1));
    CHECK(!readOnly.UpdateUInt("Channel", 99));
    CHECK(!readOnly.Delete("Channel"));

    DcMsg editable = readOnly.Clone();
    CHECK(editable.IsValid());

    uint32_t chn; std::string label;
    CHECK(editable.GetUInt("Channel", chn) && chn == 3);
    CHECK(editable.GetString("Label", label) && label == "sensor-3");

    // The clone is genuinely editable
    CHECK(editable.AddInt("New", 1));
    CHECK(editable.UpdateUInt("Channel", 99));
    CHECK(editable.Delete("Label"));

    uint32_t chn2; int32_t newVal;
    CHECK(editable.GetUInt("Channel", chn2) && chn2 == 99);
    CHECK(editable.GetInt("New", newVal) && newVal == 1);

    // Mutating the clone must not affect the original read-only source
    uint32_t chn_orig;
    std::string label_orig;
    CHECK(readOnly.GetUInt("Channel", chn_orig) && chn_orig == 3);
    CHECK(readOnly.GetString("Label", label_orig) && label_orig == "sensor-3");

    // Cloning an already-writable instance also yields an independent copy
    DcMsg clone2 = m.Clone();
    CHECK(clone2.UpdateUInt("Channel", 7));
    uint32_t orig_after;
    CHECK(m.GetUInt("Channel", orig_after) && orig_after == 3);
}

TEST(DeleteElement)
{
    DcMsg m;
    m.AddInt("a", 1);
    m.AddString("b", "middle-string");
    m.AddInt("c", 3);
    m.AddDouble("d", 4.5);

    CHECK(m.Elements() == 4);
    CHECK(m.Delete("b"));
    CHECK(m.Elements() == 3);
    CHECK(!m.Delete("b"));       // already gone
    CHECK(!m.Delete("nope"));    // never existed

    int32_t a, c; double d;
    CHECK(m.GetInt("a", a) && a == 1);
    CHECK(m.GetInt("c", c) && c == 3);
    CHECK(m.GetDouble("d", d) && d == 4.5);

    uint64_t size = 0;
    void* buf = m.GetData(size);
    DcMsg r(buf, size);
    CHECK(r.IsValid());
    CHECK(r.Elements() == 3);
    CHECK(!r.Delete("a"));   // read-only rejects delete
}

TEST(NameValidation)
{
    DcMsg m;
    CHECK(m.AddInt("dup", 1));
    CHECK(!m.AddInt("dup", 2));   // duplicate name rejected
    CHECK(m.GetLastError() == DcMsg::EError::DuplicateName);

    std::string longName(20, 'a');
    CHECK(!m.AddInt(longName.c_str(), 3));
    CHECK(m.GetLastError() == DcMsg::EError::InvalidName);
}

TEST(ErrorCodes)
{
    DcMsg m;
    int32_t v;
    CHECK(!m.GetInt("missing", v));
    CHECK(m.GetLastError() == DcMsg::EError::NotFound);

    m.AddInt("x", 1);
    double d;
    CHECK(!m.GetDouble("x", d));
    CHECK(m.GetLastError() == DcMsg::EError::TypeMismatch);

    // A subsequent success resets the error
    CHECK(m.GetInt("x", v));
    CHECK(m.GetLastError() == DcMsg::EError::None);

    // Corrupt / too-small buffer
    DcMsg broken((void*)"short", 5);
    CHECK(!broken.IsValid());
    CHECK(broken.GetLastError() == DcMsg::EError::InvalidData);

    // Null buffer pointer
    DcMsg nullBuf(nullptr, 0);
    CHECK(!nullBuf.IsValid());
    CHECK(nullBuf.GetLastError() == DcMsg::EError::InvalidArgument);

    // Empty array rejected
    DcMsg m2;
    std::vector<DcMsg> empty;
    CHECK(!m2.AddMessageArray("empty", empty));
    CHECK(m2.GetLastError() == DcMsg::EError::InvalidArgument);
}

int main()
{
    RUN(ScalarRoundTrip);
    RUN(GetWorksOnWritableInstanceDirectly);
    RUN(MemoryArrayRoundTrip);
    RUN(FixedArraysRoundTrip);
    RUN(NestedMessageAndMessageArray);
    RUN(UpdateInPlace);
    RUN(CloneReadOnlyToEditable);
    RUN(DeleteElement);
    RUN(NameValidation);
    RUN(ErrorCodes);

    printf("\n%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
