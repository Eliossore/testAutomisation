#pragma once

#include <string>

using namespace std;

namespace Utils
{
    /**
	 * @brief Retrieves the height of the terminal window.
	 *
	 * @return int The height of the terminal window.
	 */
	int GetTerminalHeight();

	/**
	 * @brief Converts a wide string to a string.
	 *
	 * @param wide_string The wide string to convert.
	 * @return string The converted string.
	 */
	string wstringToString(const wstring& wide_string);
}

