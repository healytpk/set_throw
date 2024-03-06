#include <cstdio>            // puts, printf
#include <stdexcept>         // runtime_error
#include <typeinfo>          // typeid, type_info
#include "setthrow.hpp"      // set_throw

void MyLogger(void const *const p, std::type_info const &ti) noexcept
{
    // Note that this function must be thread-safe
    // and must never throw an exception (not even
    // an exception that's caught and doesn't
    // propagate outside of this function)

    char const *str = "<no info available>";

    if ( typeid(std::runtime_error) == ti  )
    {
        str = static_cast<std::runtime_error const*>(p)->what();
    }

    std::printf("Exception logged : '%s'\n",str);
}

int main(void)
{
    std::puts("First line in main");

    std::set_throw(MyLogger);

    try
    {
        throw std::runtime_error("monkey");
    }
    catch(...)
    {
        std::puts("Exception caught");
    }

    std::puts("Last line in main");
}
