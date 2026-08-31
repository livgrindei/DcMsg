// Helper binary for the C++<->Python wire-compatibility test.
//   cross_lang_helper encode <outfile>   writes a DcMsg fixture to <outfile>
//   cross_lang_helper decode <infile>    reads <infile>, verifies it matches
//                                        the fixture the Python side writes,
//                                        prints OK/FAIL, exits 0/1
//
// tests/cross_lang/test_cross_lang.py drives both directions: it decodes the
// "encode" output itself (with the Python DcMsg port) and separately writes
// its own fixture for this binary's "decode" mode to check.

#include <dcmsg/dcmsg.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using DS::DcMsg;

static int do_encode(const char* path)
{
    DcMsg sub;
    sub.AddInt("inner", 77);
    sub.AddString("name", "child");

    DcMsg m;
    m.AddBool("b", true);
    m.AddUInt("ui", 4000000000u);
    m.AddDouble("d", -2.718281828);
    m.AddString("s", "hello from cpp");
    m.AddMessage("msg", sub);

    unsigned char mem[] = {1, 2, 3, 4, 5};
    m.AddMemoryArray("mem", mem, sizeof(mem));

    std::vector<uint32_t> uints = {1, 2, 4000000000u};
    m.AddUIntArray("uints", uints);

    std::vector<std::string> strs = {"alpha", "beta", "gamma!"};
    m.AddStringArray("strs", strs);

    DcMsg s1, s2;
    s1.AddInt("x", 1);
    s2.AddInt("x", 2);
    std::vector<DcMsg> msgs = {s1, s2};
    m.AddMessageArray("msgs", msgs);

    uint64_t size = 0;
    void* data = m.GetData(size);

    FILE* f = fopen(path, "wb");
    if (!f)
    {
        printf("FAIL: cannot open '%s' for writing\n", path);
        return 1;
    }
    fwrite(data, 1, size, f);
    fclose(f);
    return 0;
}

template <typename T>
static bool expect(const char* what, T expected, T actual)
{
    if (expected != actual)
    {
        printf("FAIL: %s\n", what);
        return false;
    }
    return true;
}

static int do_decode(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        printf("FAIL: cannot open '%s' for reading\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buf(sz);
    if (fread(buf.data(), 1, sz, f) != static_cast<size_t>(sz))
    {
        printf("FAIL: short read on '%s'\n", path);
        fclose(f);
        return 1;
    }
    fclose(f);

    DcMsg m(buf.data(), sz);
    if (!m.IsValid())
    {
        printf("FAIL: message failed to parse\n");
        return 1;
    }

    bool ok = true;

    bool b; m.GetBool("b", b); ok &= expect("b", false, b);
    uint32_t ui; m.GetUInt("ui", ui); ok &= expect("ui", 3000000000u, ui);
    double d; m.GetDouble("d", d);
    if (d < 6.0e23 || d > 6.05e23) { printf("FAIL: d\n"); ok = false; }
    std::string s; m.GetString("s", s); ok &= expect("s", std::string("hello from python"), s);

    DcMsg sub;
    m.GetMessage("msg", sub);
    int32_t inner; sub.GetInt("inner", inner); ok &= expect("msg.inner", 99, inner);
    std::string name; sub.GetString("name", name); ok &= expect("msg.name", std::string("pychild"), name);

    void* mem = nullptr; uint32_t mem_sz;
    m.GetMemoryArray("mem", &mem, mem_sz);
    unsigned char expected_mem[] = {9, 8, 7, 6};
    if (mem_sz != 4 || memcmp(mem, expected_mem, 4) != 0) { printf("FAIL: mem\n"); ok = false; }
    free(mem);

    std::vector<uint32_t> uints;
    m.GetUIntArray("uints", uints);
    std::vector<uint32_t> expected_uints = {100, 200, 300};
    if (uints != expected_uints) { printf("FAIL: uints\n"); ok = false; }

    std::vector<std::string> strs;
    m.GetStringArray("strs", strs);
    std::vector<std::string> expected_strs = {"one", "two", "three"};
    if (strs != expected_strs) { printf("FAIL: strs\n"); ok = false; }

    std::vector<DcMsg> msgs;
    m.GetMessageArray("msgs", msgs);
    if (msgs.size() != 2)
    {
        printf("FAIL: msgs size\n");
        ok = false;
    }
    else
    {
        int32_t y0, y1;
        msgs[0].GetInt("y", y0);
        msgs[1].GetInt("y", y1);
        ok &= expect("msgs[0].y", 10, y0);
        ok &= expect("msgs[1].y", 20, y1);
    }

    if (ok)
    {
        printf("OK\n");
        return 0;
    }
    return 1;
}

int main(int argc, char** argv)
{
    if (argc != 3 || (strcmp(argv[1], "encode") != 0 && strcmp(argv[1], "decode") != 0))
    {
        printf("Usage: %s <encode|decode> <path>\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "encode") == 0)
    {
        return do_encode(argv[2]);
    }
    return do_decode(argv[2]);
}
