#include "tr069/Md5.hpp"

#include <cassert>

int main() {
    assert(tr069::md5Hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    assert(tr069::md5Hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    return 0;
}
