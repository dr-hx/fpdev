#ifndef RANDOM_H
#define RANDOM_H

#include <stdlib.h>
#include <string>
#include <sstream>
#include <sys/time.h>

typedef unsigned int uint32;
typedef unsigned long uint64;

static uint64 count = 0;
std::string randomIdentifier(const std::string &base) {
    timeval time;
    gettimeofday(&time, nullptr);
    unsigned int ns = time.tv_sec*1000000 + time.tv_usec;
    std::stringstream ss;
    ss << base <<"_" << ns <<"_" << count++;
    return ss.str();
}
#endif