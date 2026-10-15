#include "../src/Range.hpp"
#include <cstdlib>
#include <iostream>
void check(bool value) { if(!value) std::abort(); }
int main() {
    using namespace partprac;
    Range range{1200,75,1800,100};
    check(validStartPosition("1,31,2,1200,3,75,kA9,1;",range));
    check(!validStartPosition("1,0,2,1200,3,75,kA9,1;",range));
    check(!validStartPosition("2,1200,3,75,kA9,1;",range));
    check(validStartPosition("1,31,2,0,3,0,kA9,1;",range)); // GD coordinates need not equal drag coordinates
    check(!validStartPosition("1,31,2,1200,3,75,kA21,1;",range));
    check(validStartPosition("3,75,2,1200,1,31,kA21,0;",range));
    check(!validStartPosition("1,31,2,1200garbage,3,75",range));
    check(!validStartPosition("1,31,2,1200,3,-75",range));
    check(!validStartPosition("1,31,2,1200,kA9,1",range));
    check(!validStartPosition("1,31,2,nan,3,75",range));
    auto full=withoutStartPositions("kA13,1;1,31,2,50,3,15;1,901,2,25,51,4;")
        +"1,31,2,1200,3,75,kA9,1;";
    check(full=="kA13,1;1,901,2,25,51,4;1,31,2,1200,3,75,kA9,1;");
    check(validStartPosition("1,31,2,4247.34,3,237.563,36,1,kA2,0,kA21,0",range));
    std::cout<<"12 start-marker validation and level assembly checks passed\n";
}
