#include "../src/ExitGate.hpp"
#include <cstdlib>
#include <iostream>
using partprac::ExitGate;
void check(bool ok) { if(!ok) std::abort(); }
int main() {
    ExitGate g;
    using A=ExitGate::Action;
    check(g.advance(true,false,false)==A::None);
    g.arm(A::RestoreSelector);
    check(g.advance(false,false,false)==A::None); // native pop hasn't run
    check(g.advance(true,true,false)==A::None); // old gameplay still alive
    check(g.advance(true,false,false)==A::None); // wait an extra frame
    check(g.advance(true,false,false)==A::RestoreSelector);
    check(g.advance(true,false,false)==A::None); // exactly once
    g.arm(A::Leave);
    check(g.advance(true,false,true)==A::None); // UI retained by other mod
    check(g.advance(true,false,false)==A::None);
    check(g.advance(true,false,false)==A::Leave);
    g.arm(A::Leave);
    check(g.advance(true,false,false)==A::None);
    check(g.advance(false,false,false)==A::None); // interrupted by scene change
    check(g.advance(true,false,false)==A::None);
    check(g.advance(true,false,false)==A::Leave);
    std::cout<<"Exit gate native teardown, delayed resume, retained UI, exactly-once checks passed\\n";
}
