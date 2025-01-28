#define WIN32_LEAN_AND_MEAN  // Prevent inclusion of unnecessary Windows headers

#include "Utils.h"
#include <Windows.h>
#include <iostream>

using namespace std;

namespace Utils
{
	// Function to get terminal size
	int GetTerminalHeight()
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
		{
			// Calculate the height of the terminal window.
			return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		}
		// Default to 24 rows if size cannot be determined.
		return 24;
	}

	string wstringToString(const wstring& wide_string)
	{
		string str;
		size_t size;
		str.resize(wide_string.length());
		wcstombs_s(&size, &str[0], str.size() + 1, wide_string.c_str(), wide_string.size());
		return str;
	}
}