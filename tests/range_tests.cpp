#include "../src/Range.hpp"
#include <iostream>
#include <limits>
#include <cstdlib>
void check(bool value,const char* name) { if(!value) { std::cerr<<name<<'\n'; std::exit(1); } }
int main() {
    using namespace partprac;
    check(Range{30,15,300,100}.valid(),"valid forward section");
    check(!Range{300,15,30,100}.valid(),"backwards section rejected");
    check(!Range{30,15,31,20}.valid(),"tiny section rejected");
    check(!Range{-30,15,300,20}.valid(),"negative start rejected");
    check(!Range{30,std::numeric_limits<float>::quiet_NaN(),300,20}.valid(),"NaN rejected");
    check(withoutStartPositions("kA13,2,kS38,1;1,31,2,90;1,901,2,10,51,3;1,1,2,50;")
        =="kA13,2,kS38,1;1,901,2,10,51,3;1,1,2,50;","outside move trigger and decorations retained");
    check(withoutStartPositions("kA13,31;2,90,1,31;1,311,2,50")
        =="kA13,31;1,311,2,50;","property matching exact and order independent");
    check(withoutStartPositions("").empty(),"empty level safe");
    std::cout<<"8 range and level-preservation checks passed\n";
}
