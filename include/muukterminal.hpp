#pragma once
#ifndef MUUK_TERMINAL_HPP
#define MUUK_TERMINAL_HPP

#include <iostream>
#include <vector>
#include <string>
#include <conio.h>   // For _getch()
#include <windows.h> // For system("cls")

#define KEY_UP 72       // Up arrow key
#define KEY_DOWN 80     // Down arrow key
#define KEY_ENTER '\r'  // Enter key

namespace muuk {
    namespace terminal {

        // Function to display a selection menu and return the selected index
        int select_from_menu(const std::vector<std::string>& options);

        // Prints a message and gets user input as a string
        std::string prompt_string(const std::string& message);

        // Prints a message and gets user input as an integer
        int prompt_int(const std::string& message);

        // Pauses execution until the user presses ENTER
        void pause(const std::string& message = "Press ENTER to continue...");

        // Waits for any key press
        void wait_for_key_press(const std::string& message = "Press any key to continue...");

    } // namespace terminal
} // namespace muuk

#endif // MUUK_TERMINAL_HPP
