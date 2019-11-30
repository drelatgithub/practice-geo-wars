#ifdef GEOWARS_BUILD_DEPENDENCY_BIN2C

// Inspired by https://gist.github.com/albertz/1551304#file-bin2c-c

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <fstream>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace std;

	if(argc != 4) {
        cout << "Usage: bin2c <input-file> <output-file> <array-name>" << endl;
        throw runtime_error("Incorrect input!");
    }
	const auto fn = argv[1];
    const auto ofn = argv[2];
    const auto an = argv[3];

    ifstream ifn(fn, ios::binary);

    ofstream ofs(ofn);

    ofs << "constexpr unsigned char " << an << "[] {";

	unsigned long n = 0;
    for(
        auto it = istreambuf_iterator<char>(ifn);
        it     != istreambuf_iterator<char>();
        ++it
    ) {
        if (n % 20 == 0) ofs << "\n    ";
        ofs << "0x" << setw(2) << setfill('0') << hex << (unsigned int)(unsigned char)(*it) << ',';
		++n;
	}
	ofs << "\n};" << endl;

    return(0);
}

#endif
