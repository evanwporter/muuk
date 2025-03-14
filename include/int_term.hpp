#include <conio.h> // For _getch()
#include <iostream>
#include <vector>
#include <windows.h> // For system("cls")

#define KEY_UP 72 // Up arrow character
#define KEY_DOWN 80 // Down arrow character
#define KEY_ENTER '\r' // Enter key character

bool selecting = true; // Global flag to handle exit

void signalHandler(int signum) {
    system("cls");
    std::cout << "\nExiting program...\n";
    exit(signum);
}

// Function to display menu with '>' for the selected option
void displayMenu(const std::vector<std::string>& options, int selected) {
    // system("cls");
    for (size_t i = 0; i < options.size(); i++) {
        std::cout << (selected == i ? "> " : "  ") << options[i] << "\n";
    }
}

// Function that allows selection from a list of options
int selectFromMenu(const std::vector<std::string>& options) {
    int selected = 0;
    int numChoices = options.size();

    displayMenu(options, selected);

    char c;
    while (selecting) {
        switch ((c = _getch())) {
        case KEY_UP:
            if (selected > 0) {
                --selected;
                displayMenu(options, selected);
            }
            break;
        case KEY_DOWN:
            if (selected < numChoices - 1) {
                ++selected;
                displayMenu(options, selected);
            }
            break;
        case KEY_ENTER:
            selecting = false;
            break;
        default:
            break;
        }
    }

    return selected; // Return the index of the selected option
}