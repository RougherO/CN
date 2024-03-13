#ifndef LOGGER
#define LOGGER

#include <cstdlib>
#include <initializer_list>
#include <iostream>

void LogToStdOut(char* const message, size_t size)
{
    std::cout.write(message, size);
    std::cout << std::endl;
}

void LogToStdOut(std::string const& message)
{
    std::cout << message << std::endl;
}

void LogToStdErr(std::string const& message)
{
    std::cerr << message << std::endl;
}

void LogToStdErrAndTerminate(std::string const& message)
{
    LogToStdErr(message);
    std::exit(-1);
}
#endif