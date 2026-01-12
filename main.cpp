#include <iostream>
#include "User.h"

using namespace std;

int main() {
    int choice;

    while (true) {
        cout << "\nWelcome To CineConnect!\n";
        cout << "What is better than watching movies? DUH! Quickly Tell me what you want to do?\n";
        cout << "1. Register\n";
        cout << "2. Login\n";
        cout << "3. Exit\n";
        cout << "Enter choice: ";
        cin >> choice;

        if (choice == 1) registerUser();
        else if (choice == 2) loginUser();
        else
            { cout << "Bye Bye, will miss you!";
              break;
            }
    }
    return 0;
}
