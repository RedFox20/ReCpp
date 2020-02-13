#pragma once
#include <cstdio>
#include <string>

struct TempFILE
{
    FILE* out = nullptr;
    TempFILE() { out = fopen("rpp_tmp_test.txt", /*truncate for binary read/write*/"w+b"); }
    ~TempFILE() { fclose(out); }
    std::string text()
    {
        fflush(out);
        fseek(out, 0, SEEK_END);
        size_t len = ftell(out);
        fseek(out, 0, SEEK_SET);
        std::string s(len, '\0');
        size_t n = fread(s.data(), 1, len, out);
        s.resize(n);
        return s;
    }
};

