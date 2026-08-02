#include "Application.h"
#include "Pch.h"

#include <iostream>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

int main()
{
    // Force Windows Console to decode stdout as UTF-8:
	#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	#endif

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