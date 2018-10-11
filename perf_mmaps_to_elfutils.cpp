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

int main(int argc, char** argv)
{
    if (argc != 2) {
        cerr << "ERROR: missing perf mmap input file, you can generate it via:\n"
             << "    $ perf record ...\n"
             << "    $ perf script --show-mmap-events  | grep PERF_RECORD_MMAP2 > mmaps.txt"
             << endl;
        return 1;
    }

    const auto mmaps = parseMmapEvents(argv[1]);

    dumpMmaps(cout, mmaps);

    return 0;
}
