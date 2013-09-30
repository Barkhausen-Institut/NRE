/*
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2011, 2012 Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Copyright (C) 1997-2009 Net Integration Technologies, Inc.
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NRE (NOVA runtime environment).
 *
 * NRE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NRE is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <stream/OStringStream.h>
#include <stream/Serial.h>
#include <Errors.h>
#include <Desc.h>
#include <cstring>

namespace nre {
namespace test {

typedef void (*test_func)();
struct TestCase {
    const char *name;
    test_func func;
};

// Whether to print info before execution of the test. Zero means
// print info after execution together with results.
#define WVTEST_PRINT_INFO_BEFORE 0

// Standard WVTEST API
#define WVSTART(title) \
    Serial::get() << "Testing \"" << title << "\" in " << nre::test::WvTest::shortpath(__FILE__) \
                  << ":" << __LINE__ << ":\n");
#define WVPASS(cond)    \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # cond); __t.check(cond); })
#define WVNOVA(novaerr) \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # novaerr); __t.check_novaerr(novaerr); })
#define WVPASSEQ(a, b)  \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # a " == " # b); __t.check_eq((a), (b), true); })
#define WVPASSEQPTR(a, b) \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # a " == " # b); \
       __t.check_eq(reinterpret_cast<uintptr_t>(a), reinterpret_cast<uintptr_t>(b), true); })
#define WVPASSLT(a, b) \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # a " < " # b); __t.check_lt((a), (b)); })
#define WVPASSGE(a, b) \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # a " <= " # b); __t.check_le((b), (a)); })
#define WVPASSLE(a, b) \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # a " <= " # b); __t.check_le((a), (b)); })
#define WVFAIL(cond)    \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, "NOT(" # cond ")"); !__t.check(!(cond)); })
#define WVFAILEQ(a, b)  \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # a " != " # b); __t.check_eq((a), (b), false); })
#define WVPASSNE(a, b)  WVFAILEQ(a, b)
#define WVFAILNE(a, b) WVPASSEQ(a, b)

// Performance monitoring
#define WVPERF(value, units)    \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, "PERF: " # value);  __t.check_perf(value, units); })

// Debugging
#define WV(code)                \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # code); __t.check(true); code; })
#define WVSHOW(val)             \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # val);  __t.show(val); })
#define WVSHOWHEX(val)          \
    ({ nre::test::WvTest __t(__FILE__, __LINE__, # val);  __t.show_hex(val); })
#define WVPRINT(expr)           \
    Serial::get() << "! " << nre::test::WvTest::shortpath(__FILE__) << ":" << __LINE__ \
                  << " " << expr << " ok\n";

class WvTest {
    const char *file, *condstr;
    int line;

    struct NovaErr {
        enum ErrorCode err;
        NovaErr(unsigned char _err)
            : err(static_cast<enum ErrorCode>(_err)) {
        }

        const char *tostr() {
            return to_string(err);
        }
    };

    const char *resultstr(bool result) {
        return result ? "ok" : "FAILED";
    }

    const char *resultstr(char *result) {
        return result;
    }

    const char *resultstr(NovaErr result) {
        return result.tostr();
    }

    void save_info(const char *_file, int _line, const char *_condstr) {
        file = _file;
        condstr = _condstr;
        line = _line;
    }

#if WVTEST_PRINT_INFO_BEFORE
    void print_info()
    {
        Serial::get() << "! " << file << ":" << line << " " << condstr << " ";
    }

    template<typename T>
    void print_result(T result, const char* suffix = "", const char *sb = "", const char *se = "")
    {
        Serial::get() << sb << suffix << se << " " << resultstr(result) << "\n";
        if(strcmp(resultstr(result), "FAILED") == 0)
            failures++;
    }
#else
    template<typename T>
    void print_result(T result, const char* suffix = "", const char *sb = "", const char *se = "") {
        Serial::get() << "! " << file << ":" << line << " " << condstr << " "
                      << sb << suffix << se << " " << resultstr(result) << "\n";
        if(strcmp(resultstr(result), "FAILED") == 0)
            failures++;
    }
#endif

    static void print_failed_cmp(const char *op, const char *a, const char *b) {
        Serial::get() << "wvtest comparison '" << a << "' " << op << " '" << b << "' FAILED\n";
    }

    static void print_failed_cmp(const char *op, unsigned a, unsigned b) {
        Serial::get() << "wvtest comparison " << a << " == " << fmt(a, "#x") << " " << op << " "
                      << b << " == " << fmt(b, "#x") << " FAILED\n";
    }

    static void print_failed_cmp(const char *op, ulong a, ulong b) {
        Serial::get() << "wvtest comparison " << a << " == " << fmt(a, "#x") << " " << op << " "
                      << b << " == " << fmt(b, "#x") << " FAILED\n";
    }

    static void print_failed_cmp(const char *op, ullong a, ullong b) {
        Serial::get() << "wvtest comparison " << a << " == " << fmt(a, "#x") << " " << op << " "
                      << b << " == " << fmt(b, "#x") << " FAILED\n";
    }

    static void print_failed_cmp(const char *op, int a, int b) {
        Serial::get() << "wvtest comparison " << a << " == " << fmt(a, "#x") << " " << op << " "
                      << b << " == " << fmt(b, "#x") << " FAILED\n";
    }

    static void stringify(char *buf, unsigned size, ullong val) {
        OStringStream(buf, size) << val;
    }

    static void stringify(char *buf, unsigned size, ulong val) {
        OStringStream(buf, size) << val;
    }

    static void stringify(char *buf, unsigned size, unsigned val) {
        OStringStream(buf, size) << val;
    }

    static void stringify(char *buf, unsigned size, int val) {
        OStringStream(buf, size) << val;
    }

    static void stringify(char *buf, unsigned size, Crd crd) {
        OStringStream os(buf, size);
        os << crd;
    }

    static void stringifyx(char *buf, unsigned size, ullong val) {
        OStringStream(buf, size) << fmt(val, "#x");
    }

    static void stringifyx(char *buf, unsigned size, ulong val) {
        OStringStream(buf, size) << fmt(val, "#x");
    }

    static void stringifyx(char *buf, unsigned size, unsigned val) {
        OStringStream(buf, size) << fmt(val, "#x");
    }

    static void stringifyx(char *buf, unsigned size, int val) {
        OStringStream(buf, size) << fmt(val, "#x");
    }

    static void stringifyx(char *buf, unsigned size, void *val) {
        OStringStream(buf, size) << val;
    }

public:
    static const char *shortpath(const char *path) {
        const char *cur = nullptr;
        const char *last = path;
        do {
            cur = strchr(last, '/');
            if(cur)
                last = cur + 1;
        }
        while(cur);
        return last;
    }

    explicit WvTest(const char *file, int line,
                    const char *condstr) : file(shortpath(file)), condstr(condstr), line(line) {
#if WVTEST_PRINT_INFO_BEFORE
        // If we are sure that nothing is printed during the "check", we can
        // print the info here, and the result after the "check" finishes.
        print_info();
#endif
    }

    bool check(bool cond, const char* suffix = "") {
        print_result(cond, suffix);
        return cond;
    }

    unsigned check_novaerr(unsigned novaerr) {
        print_result(NovaErr(novaerr));
        return novaerr;
    }

    bool check_eq(const char *a, const char *b, bool expect_equal) {
        bool result = !!strcmp(a, b) ^ expect_equal;
        if(!result)
            print_failed_cmp(expect_equal ? "==" : "!=", a, b);
        check(result);
        return result;
    }
    bool check_eq(char *a, char *b, bool expect_equal) {
        return check_eq(static_cast<const char*>(a), static_cast<const char*>(b), expect_equal);
    }
    bool check_eq(const char *a, char *b, bool expect_equal) {
        return check_eq(static_cast<const char*>(a), static_cast<const char*>(b), expect_equal);
    }
    bool check_eq(char *a, const char *b, bool expect_equal) {
        return check_eq(static_cast<const char*>(a), static_cast<const char*>(b), expect_equal);
    }

    template<typename T>
    bool check_eq(T a, T b, bool expect_equal) {
        bool result = ((a == b) ^ !expect_equal);
        if(!result)
            print_failed_cmp(expect_equal ? "==" : "!=", a, b);
        check(result);
        return result;
    }

    template<typename T>
    bool check_lt(T a, T b) {
        bool result = (a < b);
        if(!result)
            print_failed_cmp("<", a, b);
        check(result);
        return result;
    }

    template<typename T>
    bool check_le(T a, T b) {
        bool result = (a <= b);
        if(!result)
            print_failed_cmp("<=", a, b);
        check(result);
        return result;
    }

    template<typename T>
    T check_perf(T val, const char *units) {
        char valstr[20 + strlen(units)];
        stringify(valstr, sizeof(valstr), val);
        print_result(true, " ", valstr, units);
        return val;
    }

    const char *show(const char *val) {
        print_result(true, val, "= \"", "\"");
        return val;
    }

    char *show(char *val) {
        print_result(true, val, "= \"", "\"");
        return val;
    }

    template<typename T>
    T show(T val) {
        char valstr[40];
        stringify(valstr, sizeof(valstr), val);
        print_result(true, valstr, "= ");
        return val;
    }

    template<typename T>
    T show_hex(T val) {
        char valstr[40];
        stringifyx(valstr, sizeof(valstr), val);
        print_result(true, valstr, "= ");
        return val;
    }

    static size_t failures;
};

}
}
