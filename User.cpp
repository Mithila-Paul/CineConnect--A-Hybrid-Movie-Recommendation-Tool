#include "User.h"
#include "cbf.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include<cctype>

using namespace std;


// this is to generate unique user IDs, checking existing files, keeping it a bit dumb in consecutive way
int generateUserId() {
    int id = 1;
    while (true) {
        ifstream fin("user" + to_string(id) + "_ratings.txt");
        if (!fin)
            return id;
        id++;
    }
}

//registration 
void registerUser() {
    int userId = generateUserId();
    ofstream fout("user" + to_string(userId) + "_ratings.txt");
    fout.close();

    cout << "Registration successful!\n";
    cout << "Your User ID is: " << userId << endl;
}

// Login system
bool loginUser() {
    int userId;
    cout << "Enter your User ID: ";
    cin >> userId;

    string file = "user" + to_string(userId) + "_ratings.txt";
    ifstream fin(file);
        if (!fin) {
        cout << "User not found!\n";
        return false;
     }

    cout << "Login successful!\n";
    userMenu(userId);
    return true;
}

// User Menu
void userMenu(int userId) {
    int choice;
    while (true) {
        cout << "\nYou better give me some of your ratings before asking for recommendations hehe!\n";
        cout << "1. Rate a Movie\n";
        cout << "2. See My Ratings\n";
        cout << "3. Get Recommendations\n";
        cout << "4. Logout\n";
        cout << "Enter choice: ";
        cin >> choice;

        if (choice == 1) rateMovie(userId);
        else if (choice == 2) showMyRatings(userId);
        else if (choice == 3) recommendMoviesForUser(userId);
        else break;
    }
}
// keeping it case insensitive for better user experience , and this helper function as the env gets confused sometimes
        string toLower(const string& s) {
    string result = s;
    for (char& c : result)
        c = tolower(c);
    return result;
}
    
// to find movie id by its title
int findMovieIdByTitle(const string& inputTitle) {
    ifstream file("movies_processed.csv");
    string line;

    string target = toLower(inputTitle);

    getline(file, line); // skip header

    while (getline(file, line)) {
        stringstream ss(line);
        string idStr, movieTitle;

        getline(ss, idStr, ',');
        getline(ss, movieTitle, ',');

        // remove quotes if present again
        if (!movieTitle.empty() && movieTitle.front() == '"')
            movieTitle = movieTitle.substr(1, movieTitle.size() - 2);

        if (toLower(movieTitle) == target) {
            return stoi(idStr);
        }
    }
    return -1;
}


// Rate Movie and save to user's ratings file
void rateMovie(int userId) {
   
    cin.ignore(); // it helps to clear the newline character from the input buffer

    string title;
    double rating;

    cout << "Enter Movie Title: ";
    getline(cin, title);

    int movieId = findMovieIdByTitle(title);

    if (movieId == -1) {
        cout << "Movie not found!\n";
        return;
    }

    cout << "Enter Rating (1 to 5): ";
    cin >> rating;

    ofstream fout("user" + to_string(userId) + "_ratings.txt", ios::app);
    fout << movieId << " " << rating << endl;
    fout.close();

    cout << "Rating saved for \"" << title << "\"\n";
}


// to show users previous ratings
void showMyRatings(int userId) {
    ifstream fin("user" + to_string(userId) + "_ratings.txt");
    int mid;
    double r;

    cout << "\nMy Ratings:\n";
    while (fin >> mid >> r)
        cout << "Movie " << mid << " : " << r << endl;

    fin.close();
}

// the big part!
void recommendMoviesForUser(int userId) {
    auto movieVectors = loadMovieVectors("movie_vectors.txt");
    auto titles = loadMovieTitles("movies_processed.csv");

    string ratingsFile = "user" + to_string(userId) + "_ratings.txt";

    Vector userVector = buildUserVector(ratingsFile, movieVectors);
    auto rated = getRatedMovieIds(ratingsFile);

    vector<int> recs;

    if (userVector.empty()) {
        cout << "Cold start detected.\n";
        recs = coldStartRecommend(movieVectors, 10);
    } else {
        recs = recommendMovies(userVector, movieVectors, 10, rated);
    }

    cout << "\nRecommended Movies:\n";
    for (int id : recs)
        cout << id << " : " << titles[id] << endl;
}
