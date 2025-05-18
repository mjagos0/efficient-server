#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>

#ifdef _DEBUG
#define LOG_DEBUG(x) std::cout << x << std::endl
#else
#define LOG_DEBUG(x)
#endif

#endif // DEBUG_H