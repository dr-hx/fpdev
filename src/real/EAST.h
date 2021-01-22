#ifndef EAST_H
#define EAST_H
#include "ShadowExecution.hpp"


void EAST_DUMP(std::ostream& stream, double d) {} // pseudo function

void EAST_DUMP(std::ostream& stream, const SVal &d) {
    stream << d <<"\n";
}
#endif