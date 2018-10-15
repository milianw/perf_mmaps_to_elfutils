/*
  perf_mmaps_to_elfutils.cpp

  Copyright (c) 2018, Milian Wolff <mail@milianw.de>
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  The views and conclusions contained in the software and documentation are those
  of the authors and should not be interpreted as representing official policies,
  either expressed or implied, of the perf_mmaps_to_elfutils project.
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

#include <libdwfl.h>

using namespace std;

struct MmapEvent
{
    uint64_t start;
    uint64_t len;
    uint64_t pgoff;
    string flags;
    string file;
};

vector<MmapEvent> parseMmapEvents(const string& file)
{
    vector<MmapEvent> mmaps;
    ifstream input(file);
    string line;
    while (getline(input, line)) {
        MmapEvent mmap;
        char flags_buf[1024] = {};
        char file_buf[1024] = {};
        auto ret = sscanf(line.c_str(), "%*s %*u %*g: PERF_RECORD_MMAP2 %*u/%*u: [%lx(%lx) @ %lx %*u:%*u %*u %*lu]: %s %s",
               &mmap.start, &mmap.len, &mmap.pgoff, flags_buf, file_buf);
        if (ret <= 0) {
            cerr << "ERROR: unexpected mmap input line:\n" << line << endl;
            throw runtime_error("bad input");
        }

        mmap.flags = flags_buf;
        mmap.file = file_buf;
        mmaps.push_back(mmap);
    }
    return mmaps;
}

void dumpMmaps(ostream& out, const vector<MmapEvent>& mmaps)
{
    out << hex << showbase;
    for (const auto& mmap : mmaps) {
        out << mmap.start << " to " << (mmap.start + mmap.len) << ", len = " << setw(10) << mmap.len << ", offset = " << setw(16) << mmap.pgoff << "\t" << mmap.flags << "\t" << mmap.file << "\n";
    }
}

void filterMmaps(vector<MmapEvent> &mmaps)
{
    mmaps.erase(remove_if(mmaps.begin(), mmaps.end(), [](const MmapEvent& mmap) {
        return mmap.file.empty() || mmap.file[0] == '[' || mmap.file == "//anon" || mmap.file == "/etc/ld.so.cache";
    }), mmaps.end());
}

static const constexpr Dwfl_Callbacks dwfl_callbacks = {
    &dwfl_build_id_find_elf,
    &dwfl_build_id_find_debuginfo,
    &dwfl_offline_section_address,
    nullptr,
};

struct DwflHandle
{
    Dwfl* handle = nullptr;

    DwflHandle()
        : handle(dwfl_begin(&dwfl_callbacks))
    {}

    ~DwflHandle()
    {
        dwfl_end(handle);
    }

    void report(const vector<MmapEvent>& mmaps)
    {
        dwfl_report_begin(handle);

        for (const auto& mmap : mmaps)
        {
            if (mmap.pgoff > 0) {
                cerr << "skipping (pgoff != 0): " << mmap.file << "\t" << hex << mmap.pgoff << endl;
                continue;
            }

            auto *module = dwfl_report_elf(handle, mmap.file.c_str(), mmap.file.c_str(),
                                           -1, mmap.start, false);
            if (!module) {
                cerr << "failed to report " << mmap.file.c_str() << " at " << hex << mmap.start << "@" << hex << mmap.pgoff << ", error:"
                     << dwfl_errmsg(dwfl_errno()) << endl;
            } else {
                Dwarf_Addr start, end, dwbias, symbias;
                const char* mainfile = nullptr, *debugfile = nullptr;
                dwfl_module_info(module, nullptr, &start, &end, &dwbias, &symbias, &mainfile, &debugfile);
                cout << "reported module " << mmap.file.c_str() << "\n"
                     << "\texpected: " << hex << mmap.start << "@" << hex << mmap.pgoff << " to " << (mmap.start + mmap.len) << " (" << mmap.len << ")\n"
                     << "\tactual:   " << hex << start << "@" << hex << 0 << " to " << end << " (" << (end - start) << ")\n"
                     << "\tdwbias:   " << hex << dwbias << ", symbias:" << hex << symbias << "\n"
                     << "\tmainfile: " << (mainfile ? mainfile : "<no main file>") << "\n"
                     << "\tdbgfile:  " << (debugfile ? debugfile : "<no debug file>") << "\n";
                if (mmap.start != start) {
                    cerr << "START ADDR MISMATCH! " << hex << mmap.start << " VS " << hex << start << endl;
                }
                if (mmap.len != (end - start)) {
                    cerr << "LEN MISMATCH! " << hex << mmap.len << " VS " << hex << (end - start) << ", diff: " << dec << static_cast<ssize_t>((end - start) - mmap.len) << endl;
                }
            }
        }

        dwfl_report_end(handle, nullptr, nullptr);
    }
};

int main(int argc, char** argv)
{
    if (argc != 2) {
        cerr << "ERROR: missing perf mmap input file, you can generate it via:\n"
             << "    $ perf record ...\n"
             << "    $ perf script --show-mmap-events  | grep PERF_RECORD_MMAP2 > mmaps.txt"
             << endl;
        return 1;
    }

    auto mmaps = parseMmapEvents(argv[1]);
    dumpMmaps(cout, mmaps);

    cout << "\nfiltered:\n\n";

    filterMmaps(mmaps);
    dumpMmaps(cout, mmaps);

    cout << "\nreporting filtered mmaps:\n\n";

    DwflHandle dwfl;
    dwfl.report(mmaps);

    return 0;
}
