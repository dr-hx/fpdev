#include <stdlib.h>
#include <string>
#include <sstream>
#include <sys/time.h>

static int count = 0;
std::string randomIdentifier(const std::string &base) {
    timeval time;
    gettimeofday(&time, NULL);
    unsigned int ns = time.tv_sec*1000000 + time.tv_usec;
    std::stringstream ss;
    ss << base <<"_" << ns <<"_" << count++;
    return ss.str();
}