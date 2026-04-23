#include <iostream>
#include <limits>
#include "User.h"
#include "cbf.h"
#include "trends.h"
#include "system_init.h"

using namespace std;

int main()
{
    int choice;

    initializeSystem();

    cout << "Computing current trends...\n";
    TrendsData globalTrends = computeTrends();

    while (true)
    {
        cout << "\n========================================\n";
        cout << "   Welcome to CineConnect!\n";
        cout << "   Your hybrid movie recommender\n";
        cout << "========================================\n";
        cout << "1. Register\n";
        cout << "2. Login\n";
        cout << "3. Exit\n";
        cout << "Enter choice: ";

        cin >> choice;

        if (cin.fail())
        {
            cout << "Invalid input! Please enter a number.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        if (choice == 1)
        {
            registerUser(globalTrends);
        }
        else if (choice == 2)
        {
            loginUser(globalTrends);
        }
        else if (choice == 3)
        {
            cout << "Bye Bye, will miss you!";
            break;
        }
        else
        {
            cout << "Ohho! Invalid Choice! Try again";
            continue;
        }
    }

    return 0;
}
