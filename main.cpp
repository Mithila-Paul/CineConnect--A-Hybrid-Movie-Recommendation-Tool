#include <iostream>
#include "User.h"
using namespace std;

int main() {
    int choice;
    while(true) {
        cout << "\nWelcome to CineConnect! \n";
        cout << "Kindly Select your \n";
        cout << "1. Register\n";
        cout << "2. Login\n";
        cout << "3. Exit\n";
        cout << "Enter choice: ";
        cin >> choice;

        if(choice == 1) registerUser();
        else if(choice == 2) loginUser();
        else break;
    }
    return 0;
}
