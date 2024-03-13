#ifndef LOGGER
#define LOGGER

#include <initializer_list>
#include <iostream>

void LogToStdOut(char const* message, size_t size, char const* delim = "\n")
{
    std::cout.write(message, size) << delim << std::ends;
}

int32_t LogFromStdIn(char* message_ptr, size_t size)
{
    return std::cin.getline(message_ptr, size).gcount();
}

void LogToStdOut(std::string const& message, char const* delim = "\n")
{
    std::cout << message << delim << std::ends;
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