#include <conio.h>
#include <iostream>
#include <string>

#include "muukterminal.hpp"

namespace muuk {
    namespace terminal {

        // TODO: Replace colors with this
        // constexpr const char* WARN_PREFIX = "\033[1;33mwarning:\033[0m "; // Yellow
        // constexpr const char* ERROR_PREFIX = "\033[1;31merror:\033[0m "; // Red

        std::string get_indent() {
            return std::string(
                current_indent_level,
                ' ');
        }

        void move_cursor_up(int lines) {
            for (int i = 0; i < lines; i++) {
                std::cout << "\x1b[A"; // Move cursor up one line (ANSI escape sequence)
            }
        }

        void display_menu(const std::vector<std::string>& options, int selected) {
            for (size_t i = 0; i < options.size(); i++) {
                std::cout << (selected == static_cast<int>(i) ? "> " : "  ") << options[i] << "\n";
            }
        }

        int select_from_menu(const std::vector<std::string>& options) {
            int selected = 0;
            int numChoices = options.size();
            bool selecting = true;

            // Initial display
            display_menu(options, selected);
            int c;

            while (selecting) {
                c = _getch(); // Capture key press

                switch (c) {
                case 72: // UP arrow key (ASCII code for UP in _getch())
                    if (selected > 0) {
                        --selected;
                        move_cursor_up(numChoices); // Move cursor back up to the top of the menu
                        display_menu(options, selected); // Redraw
                    }
                    break;
                case 80: // DOWN arrow key (ASCII code for DOWN in _getch())
                    if (selected < numChoices - 1) {
                        ++selected;
                        move_cursor_up(numChoices);
                        display_menu(options, selected);
                    }
                    break;
                case 13: // ENTER key
                    selecting = false;
                    break;
                default:
                    break;
                }
            }
            return selected;
        }

        std::string prompt_string(const std::string& message) {
            std::string input;
            std::cout << message << ": ";
            std::getline(std::cin, input);
            return input;
        }

        int prompt_int(const std::string& message) {
            std::string input;
            int value;
            while (true) {
                std::cout << message << ": ";
                std::getline(std::cin, input);
                try {
                    value = std::stoi(input);
                    return value;
                } catch (...) {
                    std::cout << "Invalid input. Please enter a valid number.\n";
                }
            }
        }

        void print_divider(const std::string& title) {
            std::cout << "\n------------------------------------------\n";
            if (!title.empty()) {
                std::cout << " " << title << "\n";
                std::cout << "------------------------------------------\n";
            }
        }

        void pause(const std::string& message) {
            std::cout << message;
            std::cin.get(); // Waits for ENTER
        }

        void wait_for_key_press(const std::string& message) {
            std::cout << message;
            _getch(); // Waits for any key press
        }
    } // namespace terminal
} // namespace muuk