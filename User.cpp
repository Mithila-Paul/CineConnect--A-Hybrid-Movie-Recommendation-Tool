#include "User.h"
#include "cbf.h"
#include "tf-idf.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>
#include <limits>
#include <algorithm> // Added for transform

using namespace std;

// this is to generate unique user IDs
int generateUserId()
{
    int id = 1;
    while (true)
    {
        ifstream fin("user" + to_string(id) + "_ratings.txt");
        if (!fin)
            return id;
        id++;
    }
}

// registration
void registerUser()
{
    int userId = generateUserId();
    ofstream fout("user" + to_string(userId) + "_ratings.txt");
    fout.close();

    cout << "Registration successful!\n";
    cout << "Your User ID is: " << userId << endl;
}

// Login system
bool loginUser()
{
    int userId;
    cout << "Enter your User ID: ";
    cin >> userId;

    string file = "user" + to_string(userId) + "_ratings.txt";
    ifstream fin(file);
    if (!fin)
    {
        cout << "User not found!\n";
        return false;
    }

    cout << "Login successful!\n";
    userMenu(userId);
    return true;
}

// User Menu
void userMenu(int userId)
{
    int choice;
    while (true)
    {
        cout << "\nYour Menu:\n";
        cout << "1. Rate a Movie\n";
        cout << "2. See My Ratings\n";
        cout << "3. Get Recommendations\n";
        cout << "4. Logout\n";
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
            rateMovie(userId);
        else if (choice == 2)
            showMyRatings(userId);
        else if (choice == 3)
            recommendMoviesForUser(userId);
        else if (choice == 4)
            break;
        else
        {
            cout << "Invalid Choice";
            continue;
        }
    }
}

// Helper for lower case
string toLower(const string &s)
{
    string result = s;
    for (char &c : result)
        c = tolower(c);
    return result;
}

// Find movie ID
int findMovieIdByTitle(const string &inputTitle)
{
    ifstream file("movies_processed.csv");
    string line;
    string target = toLower(inputTitle);

    getline(file, line); // skip header

    while (getline(file, line))
    {
        stringstream ss(line);
        string idStr, movieTitle;

        getline(ss, idStr, ',');
        getline(ss, movieTitle, ',');

        if (!movieTitle.empty() && movieTitle.front() == '"')
            movieTitle = movieTitle.substr(1, movieTitle.size() - 2);

        if (toLower(movieTitle) == target)
        {
            return stoi(idStr);
        }
    }
    return -1;
}

string getMovieTitleById(int movieId)
{
    ifstream file("movies_processed.csv");
    string line;

    if (!file.is_open())
        return "Unknown Title";

    getline(file, line);

    while (getline(file, line))
    {
        stringstream ss(line);
        string idStr, title;

        getline(ss, idStr, ',');
        getline(ss, title, ',');

        if (stoi(idStr) == movieId)
        {
            if (!title.empty() && title.front() == '"')
                title = title.substr(1, title.size() - 2);
            return title;
        }
    }
    return "Unknown Title";
}

void rateMovie(int userId)
{
    cin.ignore();
    string title;
    double rating;

    cout << "Enter Movie Title: ";
    getline(cin, title);

    int movieId = findMovieIdByTitle(title);

    if (movieId == -1)
    {
        cout << "Movie not found!\n";
        return;
    }

    string movieTitle = getMovieTitleById(movieId);

    cout << "Enter Rating (1 to 5): ";
    cin >> rating;
    if (rating < 1 || rating > 5)
    {
        cout << "Invalid rating! Must be between 1 and 5.\n";
        return;
    }

    ofstream fout("user" + to_string(userId) + "_ratings.txt", ios::app);
    fout << movieId << " | " << movieTitle << " | " << rating << endl;
    fout.close();

    cout << "Rating saved for \"" << title << "\"\n";
}

void showMyRatings(int userId)
{
    ifstream fin("user" + to_string(userId) + "_ratings.txt");

    if (!fin.is_open())
    {
        cout << "No ratings found.\n";
        return;
    }

    int movieId;
    string title;
    string bar;
    double rating;

    cout << "\nMy Ratings:\n";

    while (fin >> movieId >> bar)
    {
        getline(fin, title, '|');
        fin >> rating;

        if (!title.empty() && title[0] == ' ')
            title.erase(0, 1);

        cout << title << " (" << movieId << ") : " << rating << "/5\n";
    }

    fin.close();
}

void recommendMoviesForUser(int userId)
{
    auto movieVectors = loadMovieVectors("movie_vectors.txt");
    auto titles = loadMovieTitles("movies_processed.csv");
    auto movieTopics = loadMovieTopics("movie_topics.txt");

    string ratingsFile = "user" + to_string(userId) + "_ratings.txt";

    cin.ignore();
    cout << "What kind of movie are you craving right now?\n";

    string query;
    getline(cin, query);

    // for query vector
    vector<vector<string>> docs;
    for (auto &mv : movieVectors)
    {
        vector<string> words;
        for (auto &it : mv.second)
            words.push_back(it.first);

        docs.push_back(words);
    }

    auto df = computeDF(docs);
    Vector queryVector = buildQueryVector(query, df, movieVectors.size());

    auto queryTopics = processQuery(query);

    Vector userVector = buildUserVector(ratingsFile, movieVectors);
    auto rated = getRatedMovieIds(ratingsFile);

    vector<int> recs;

    if (rated.empty())
    {
        cout << "\nHello new User!\n";
        // Pass queryVector here
        recs = coldStartRecommend(movieVectors, queryVector, 10);
    }
    else
    {
        // Added queryTopics to the function call
        recs = recommendMovies(
            userVector,
            queryVector,
            movieVectors,
            10,
            rated,
            queryTopics);
    }

    cout << "\nRecommended Movies:\n";
    for (int id : recs)
        cout << id << " : " << titles[id] << endl;
}
