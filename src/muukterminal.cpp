#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <iostream>
#include <string>

#include "muukterminal.hpp"

namespace muuk {
    namespace terminal {

        // TODO: Replace colors with this
        // constexpr const char* WARN_PREFIX = "\033[1;33mwarning:\033[0m "; // Yellow
        // constexpr const char* ERROR_PREFIX = "\033[1;31merror:\033[0m "; // Red

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

#ifndef _WIN32
        // Unix/macOS equivalent to _getch()
        int getch() {
            struct termios oldt, newt;
            int ch;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            ch = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return ch;
        }

        // Optional: handle escape sequences for arrow keys
        int get_arrow_key() {
            int c = getch();
            if (c == 27) { // Escape
                if (getch() == 91) { // [
                    int dir = getch();
                    switch (dir) {
                    case 'A':
                        return 72; // Up
                    case 'B':
                        return 80; // Down
                    default:
                        return dir;
                    }
                }
            }
            return c;
        }
#else
        int get_arrow_key() {
            return _getch();
        }
#endif

        int select_from_menu(const std::vector<std::string>& options) {
            int selected = 0;
            int numChoices = static_cast<int>(options.size());
            bool selecting = true;

            // Initial display
            display_menu(options, selected);
            int c;

            while (selecting) {
                c = get_arrow_key(); // Capture key press

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
                case 10: // ENTER on Unix
                case 13: // ENTER on Windows
                    selecting = false;
                    break;
                default:
                    break;
                }
            }

            return selected;
        }

        void pause(const std::string& message) {
            std::cout << message;
            std::cin.get(); // Waits for ENTER
        }

        void wait_for_key_press(const std::string& message) {
            std::cout << message;
#ifdef _WIN32
            _getch(); // Waits for any key press
#else
            getch(); // Waits for any key press
#endif
        }

    } // namespace terminal
} // namespace muuk
