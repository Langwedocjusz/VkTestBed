#include "Application.h"
#include "Pch.h"

#include <iostream>

int main()
{
    try
    {
        Application app{};
        app.Run();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}