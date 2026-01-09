#include <iostream>
using namespace std;

    void registerUser() {
    string uid, name;
    cout << "Enter new User ID: ";
    cin >> uid;

    ofstream fout("user_" + uid + ".txt");
    fout << uid << endl;
    cout << "Enter Name: ";
    cin >> name;
    fout << name << endl;
    fout.close();

    cout << "Registration Successful!" << endl;
  }

  bool loginUser() {
    string uid;
    cout << "Enter User ID: ";
    cin >> uid;

    ifstream fin("user_" + uid + ".txt");
    if(!fin) {
        cout << "User not found!\n";
        return false;
    }
    cout << "Login Successful!" << endl;

    userMenu(uid);
    fin.close();
    return true;
  }

  void userMenu(string uid) {
    int c;
    while(true) {
        cout << "\n User Menu: \n";
        cout << "1. Rate a Movie\n";
        cout << "2. See My Ratings\n";
        cout << "3. Recommend Movies\n";
        cout << "4. Statistics\n";
        cout << "5. Logout\n";
        cin >> c;

        if(c == 1) rateMovie(uid);
        else if(c == 2) showMyRatings(uid);
        else if(c == 3) recommend(uid);
        else if(c == 4) showStats();
        else break;
    }
}
