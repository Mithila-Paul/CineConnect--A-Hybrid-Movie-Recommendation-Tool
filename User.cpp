#include "User.h"
#include "cbf.h"
#include "tf-idf.h"
#include "cf.h"
#include "genre.h"
#include "mf.h"
#include "gp.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>
#include <limits>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <iomanip>

using namespace std;

struct MovieStats
{
    double averageRating = 0.0;
    int ratingCount = 0;
};

// Reads ratings_processed.csv and computes per-movie average + count
unordered_map<int, MovieStats> computeMovieStats(const string &filename)
{
    unordered_map<int, double> sumRatings;
    unordered_map<int, int> countRatings;
    unordered_map<int, MovieStats> stats;

    ifstream file(filename);
    if (!file.is_open())
    {
        cout << "Could not open " << filename << " for movie statistics.\n";
        return stats;
    }

    string line;
    getline(file, line); // skip header

    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        stringstream ss(line);
        string movieIdStr, userIdStr, ratingStr;
        getline(ss, movieIdStr, ',');
        getline(ss, userIdStr, ',');
        getline(ss, ratingStr, ',');

        if (movieIdStr.empty() || ratingStr.empty()) continue;

        try
        {
            int movieId = stoi(movieIdStr);
            double rating = stod(ratingStr);

            sumRatings[movieId] += rating;
            countRatings[movieId]++;
        }
        catch (...) {}
    }

    file.close();

    for (auto &kv : countRatings)
    {
        int movieId = kv.first;
        int cnt = kv.second;

        MovieStats ms;
        ms.ratingCount = cnt;
        ms.averageRating = (cnt > 0) ? (sumRatings[movieId] / cnt) : 0.0;
        stats[movieId] = ms;
    }

    return stats;
}

string getMovieTitleById(int movieId);

// Returns the top N seed movies for a genre using popularity-weighted score
vector<int> getTopSeedMoviesByGenre(const string &genre, int count)
{
    GenreMap gmap = loadGenreMap("movie_genres.txt");
    auto titles = loadMovieTitles("movies_synchronized.csv");
    auto stats = computeMovieStats("ratings_processed.csv");

    // strict primary-genre candidates first
    vector<int> candidates = getMoviesByPrimaryGenre(genre, gmap);

    // if too few, expand using threshold
    if ((int)candidates.size() < count)
    {
        auto expanded = getMoviesByGenreThreshold(genre, gmap, 0.15);
        unordered_set<int> seen(candidates.begin(), candidates.end());
        for (int id : expanded)
        {
            if (!seen.count(id))
            {
                candidates.push_back(id);
                seen.insert(id);
            }
        }
    }

    vector<pair<double, int>> ranked;

    for (int movieId : candidates)
    {
        if (!titles.count(movieId)) continue;

        double avg = 0.0;
        int cnt = 0;

        if (stats.count(movieId))
        {
            avg = stats[movieId].averageRating;
            cnt = stats[movieId].ratingCount;
        }

        // popularity-weighted score
        double score = avg * log(1.0 + cnt);

        // if a movie has no ratings, still keep it with very low score
        ranked.push_back({score, movieId});
    }

    sort(ranked.begin(), ranked.end(),
         [](const pair<double, int> &a, const pair<double, int> &b)
         {
             if (fabs(a.first - b.first) > 1e-9)
                 return a.first > b.first;
             return a.second < b.second;
         });

    vector<int> result;
    unordered_set<int> used;

    for (auto &p : ranked)
    {
        if ((int)result.size() >= count) break;
        if (used.count(p.second)) continue;
        result.push_back(p.second);
        used.insert(p.second);
    }

    return result;
}

vector<string> chooseFavoriteGenres(int maxGenres)
{
    vector<string> chosen;
    unordered_set<string> used;

    cout << "\nChoose up to " << maxGenres << " favorite genres.\n";
    cout << "Enter 0 when finished.\n\n";

    for (int i = 0; i < (int)GENRE_DISPLAY.size(); i++)
        cout << setw(2) << (i + 1) << ". " << GENRE_DISPLAY[i].second << "\n";

    while ((int)chosen.size() < maxGenres)
    {
        cout << "\nPick genre #" << (chosen.size() + 1) << " by number (0 to stop): ";
        int choice;
        cin >> choice;

        if (cin.fail())
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input. Try again.\n";
            continue;
        }

        if (choice == 0) break;

        if (choice < 1 || choice > (int)GENRE_DISPLAY.size())
        {
            cout << "Invalid genre number.\n";
            continue;
        }

        string genre = GENRE_DISPLAY[choice - 1].first;
        if (used.count(genre))
        {
            cout << "You already selected that genre.\n";
            continue;
        }

        chosen.push_back(genre);
        used.insert(genre);
        cout << "Added: " << GENRE_DISPLAY[choice - 1].second << "\n";
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return chosen;
}

void saveSeedRating(int userId, int movieId, double rating)
{
    string movieTitle = getMovieTitleById(movieId);

    ofstream fout("user" + to_string(userId) + "_ratings.txt", ios::app);
    fout << movieId << " | " << movieTitle << " | " << rating << "\n";
    fout.close();

    appendToGlobalRatings(userId, movieId, rating);
}

string getGenreDisplayName(const string &genreKey)
{
    for (const auto &p : GENRE_DISPLAY)
    {
        if (p.first == genreKey)
            return p.second;
    }
    return genreKey;
}

void runColdStartOnboarding(int userId)
{
    cout << "\n==============================\n";
    cout << "Cold-Start Onboarding\n";
    cout << "==============================\n";
    cout << "Help us know your taste better.\n";
    cout << "Choose favorite genres and rate a few seed movies.\n";

    vector<string> favoriteGenres = chooseFavoriteGenres(3);

    if (favoriteGenres.empty())
    {
        cout << "No genres selected. Skipping onboarding.\n";
        return;
    }

    auto titles = loadMovieTitles("movies_synchronized.csv");
    unordered_set<int> alreadyShown;

    for (const string &genre : favoriteGenres)
    {
        vector<int> seeds = getTopSeedMoviesByGenre(genre, 3);

        if (seeds.empty())
        {
            cout << "\nNo seed movies found for genre: " << genre << "\n";
            continue;
        }

        cout << "\n----------------------------------\n";
        cout << "Genre: " << getGenreDisplayName(genre) << "\n";
        cout << "Here are 3 seed movies for this genre:\n";

        vector<int> uniqueSeeds;
        for (int movieId : seeds)
        {
            if (alreadyShown.count(movieId)) continue;
            if (!titles.count(movieId)) continue;

            uniqueSeeds.push_back(movieId);
            if ((int)uniqueSeeds.size() == 3) break;
        }

        if (uniqueSeeds.empty())
        {
            cout << "All seed movies for this genre were already shown. Skipping.\n";
            continue;
        }

        for (int i = 0; i < (int)uniqueSeeds.size(); i++)
        {
            int movieId = uniqueSeeds[i];
            cout << "  " << (i + 1) << ". " << titles[movieId] << "\n";
        }

        cout << "\nEnter:\n";
        cout << "  r = rate these movies\n";
        cout << "  s = skip this genre\n";
        cout << "  q = quit onboarding\n";
        cout << "Your choice: ";

        char genreChoice;
        cin >> genreChoice;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (genreChoice == 'q' || genreChoice == 'Q')
        {
            cout << "Exiting onboarding early.\n";
            break;
        }

        if (genreChoice == 's' || genreChoice == 'S')
        {
            cout << "Skipped genre: " << genre << "\n";
            continue;
        }

        if (!(genreChoice == 'r' || genreChoice == 'R'))
        {
            cout << "Invalid choice. Skipping this genre.\n";
            continue;
        }

        for (int movieId : uniqueSeeds)
        {
            alreadyShown.insert(movieId);

            cout << "\n" << titles[movieId] << "\n";
            cout << "Rate from 1 to 5\n";
            cout << "Enter 0 to skip this movie: ";

            double rating;
            cin >> rating;

            while (cin.fail() || rating < 0 || rating > 5)
            {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Enter a valid rating between 0 and 5: ";
                cin >> rating;
            }

            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (rating == 0)
            {
                cout << "Skipped.\n";
                continue;
            }

            if (!hasRatedMovie(userId, movieId))
            {
                saveSeedRating(userId, movieId, rating);
                cout << "Saved.\n";
            }
            else
            {
                cout << "Already rated, skipped.\n";
            }
        }
    }

    cout << "\nOnboarding complete. Your profile has been seeded.\n";
}

// this is to generate unique user IDs
// Pre-existing MovieLens users occupy IDs 1-610 in ratings_processed.csv
// New users start from 611 onwards
int generateUserId()
{
    // First, find the highest userId already present in the global ratings file
    int maxExisting = 610; // minimum floor — MovieLens dataset ceiling
    ifstream globalRatings("ratings_processed.csv");
    if (globalRatings.is_open())
    {
        string line;
        getline(globalRatings, line); // skip header
        while (getline(globalRatings, line))
        {
            stringstream ss(line);
            string movieIdStr, userIdStr;
            getline(ss, movieIdStr, ',');
            getline(ss, userIdStr, ',');
            if (!userIdStr.empty())
            {
                try {
                    int uid = stoi(userIdStr);
                    if (uid > maxExisting) maxExisting = uid;
                } catch (...) {}
            }
        }
        globalRatings.close();
    }

    // Now find the next ID that has no per-user file either
    int id = maxExisting + 1;
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

    runColdStartOnboarding(userId);
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
    ifstream file("movies_synchronized.csv");
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
    ifstream file("movies_synchronized.csv");
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

// Check if user already rated a specific movie
// Checks BOTH per-user file (new users) AND global CSV (MovieLens users 1-610)
bool hasRatedMovie(int userId, int movieId)
{
    // 1. Check per-user file (robust line-by-line parsing to handle \r\n)
    ifstream fin("user" + to_string(userId) + "_ratings.txt");
    if (fin.is_open())
    {
        string line;
        while (getline(fin, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            stringstream ss(line);
            string midStr, bar, title, rStr;
            getline(ss, midStr, '|');
            getline(ss, title,  '|');
            getline(ss, rStr,   '|');
            midStr.erase(remove_if(midStr.begin(), midStr.end(), ::isspace), midStr.end());
            if (!midStr.empty())
            {
                try { if (stoi(midStr) == movieId) { fin.close(); return true; } }
                catch (...) {}
            }
        }
        fin.close();
    }

    // 2. For MovieLens users (1-610), also check global CSV
    if (userId >= 1 && userId <= 610)
    {
        ifstream global("ratings_processed.csv");
        if (global.is_open())
        {
            string line;
            getline(global, line); // skip header
            while (getline(global, line))
            {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                stringstream ss(line);
                string mStr, uStr;
                getline(ss, mStr, ',');
                getline(ss, uStr, ',');
                if (!uStr.empty() && !mStr.empty())
                {
                    try {
                        if (stoi(uStr) == userId && stoi(mStr) == movieId)
                        { global.close(); return true; }
                    } catch (...) {}
                }
            }
            global.close();
        }
    }
    return false;
}

// Remove a specific (userId, movieId) entry from the global ratings CSV
// Done by writing all other lines to a temp file then renaming (atomic replace)
void removeFromGlobalRatings(int userId, int movieId)
{
    ifstream fin("ratings_processed.csv");
    ofstream tmp("ratings_processed_tmp.csv");
    if (!fin.is_open() || !tmp.is_open()) return;

    string line;
    getline(fin, line);
    // Write header without \r so the output file is clean
    if (!line.empty() && line.back() == '\r') line.pop_back();
    tmp << line << "\n";

    while (getline(fin, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // strip Windows \r
        if (line.empty()) continue;

        stringstream ss(line);
        string midStr, uidStr, rStr;
        getline(ss, midStr, ',');
        getline(ss, uidStr, ',');
        getline(ss, rStr,   ',');

        bool isTarget = false;
        try {
            isTarget = (stoi(midStr) == movieId && stoi(uidStr) == userId);
        } catch (...) {}

        if (!isTarget)
            tmp << line << "\n"; // keep all lines EXCEPT the matching one
    }
    fin.close();
    tmp.close();
    remove("ratings_processed.csv");
    rename("ratings_processed_tmp.csv", "ratings_processed.csv");
}

// Remove a specific movieId from the per-user ratings file
// Uses line-by-line parsing to handle Windows \r\n line endings correctly
void removeFromUserFile(int userId, int movieId)
{
    string filename = "user" + to_string(userId) + "_ratings.txt";
    ifstream fin(filename);
    ofstream tmp("user_tmp.txt");
    if (!fin.is_open() || !tmp.is_open()) return;

    string line;
    while (getline(fin, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        stringstream ss(line);
        string midStr, title, rStr;
        getline(ss, midStr, '|');
        getline(ss, title,  '|');
        getline(ss, rStr,   '|');
        midStr.erase(remove_if(midStr.begin(), midStr.end(), ::isspace), midStr.end());
        if (midStr.empty()) continue;
        try {
            if (stoi(midStr) == movieId) continue; // skip — this is the one to remove
        } catch (...) { continue; }
        tmp << line << "\n"; // keep everything else exactly as-is
    }
    fin.close();
    tmp.close();
    remove(filename.c_str());
    rename("user_tmp.txt", filename.c_str());
}

// Append one rating to the global ratings_processed.csv
void appendToGlobalRatings(int userId, int movieId, double rating)
{
    ofstream fout("ratings_processed.csv", ios::app);
    fout << movieId << "," << userId << "," << rating << "\n";
    fout.close();
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

    // Check for existing rating
    if (hasRatedMovie(userId, movieId))
    {
        cout << "You already rated \"" << movieTitle << "\".\n";
        cout << "Do you want to replace your rating? (y/n): ";
        char choice;
        cin >> choice;
        if (choice != 'y' && choice != 'Y') return;

        // Delete old entry from both places
        removeFromUserFile(userId, movieId);
        removeFromGlobalRatings(userId, movieId);
        cout << "Old rating removed.\n";
    }

    cout << "Enter Rating (1 to 5): ";
    cin >> rating;
    if (rating < 1 || rating > 5)
    {
        cout << "Invalid rating! Must be between 1 and 5.\n";
        return;
    }

    // Write to per-user file (human-readable, for display)
    ofstream fout("user" + to_string(userId) + "_ratings.txt", ios::app);
    fout << movieId << " | " << movieTitle << " | " << rating << "\n";
    fout.close();

    // Write to global ratings CSV (for CF, MF, GP to use)
    appendToGlobalRatings(userId, movieId, rating);

    cout << "Rating saved for \"" << movieTitle << "\"!\n";
}

void showMyRatings(int userId)
{
    ifstream fin("user" + to_string(userId) + "_ratings.txt");

    if (!fin.is_open())
    {
        cout << "No ratings found.\n";
        return;
    }

    cout << "\nMy Ratings:\n";

    string line;
    unordered_set<int> seen; // guard against duplicate entries in file
    while (getline(fin, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        stringstream ss(line);
        string midStr, title, rStr;
        getline(ss, midStr, '|');
        getline(ss, title,  '|');
        getline(ss, rStr,   '|');

        // strip leading/trailing whitespace
        midStr.erase(remove_if(midStr.begin(), midStr.end(), ::isspace), midStr.end());
        rStr.erase(  remove_if(rStr.begin(),   rStr.end(),   ::isspace), rStr.end());
        if (!title.empty() && title.front() == ' ') title.erase(0, 1);
        if (!title.empty() && title.back()  == ' ') title.pop_back();

        if (midStr.empty() || rStr.empty()) continue;

        try {
            int mid = stoi(midStr);
            if (seen.count(mid)) continue; // skip duplicate entries
            seen.insert(mid);
            double r = stod(rStr);
            cout << "  " << title << " (" << mid << ") : " << r << "/5\n";
        } catch (...) { continue; }
    }

    fin.close();
}

void recommendMoviesForUser(int userId)
{
    // ── Load core data ────────────────────────────────────────────────────────
    auto movieVectors   = loadMovieVectors("movie_vectors.txt");
    auto titles         = loadMovieTitles("movies_synchronized.csv");
    auto movieTopicsMap = loadMovieTopics("movie_topics.txt");
    GenreMap gmap       = loadGenreMap("movie_genres.txt");

    string ratingsFile = "user" + to_string(userId) + "_ratings.txt";
    auto   rated       = getRatedMovieIds(ratingsFile, userId);

    // ── Search mode selection ─────────────────────────────────────────────────
    cout << "\nHow do you want to search?\n";
    cout << "  1. By genre\n";
    cout << "  2. By actor\n";
    cout << "  3. By director\n";
    cout << "  4. By plot / keywords\n";
    cout << "  5. General craving (free text)\n";
    cout << "Enter choice: ";

    int searchMode;
    cin >> searchMode;
    if (cin.fail()) { cin.clear(); cin.ignore(10000, '\n'); searchMode = 5; }
    cin.ignore(10000, '\n');

    // ── Candidate set: start with all movies, then filter by search ───────────
    vector<int> candidateIds;
    string searchGenre;

    if (searchMode == 1)
    {
        cout << "\nAvailable genres:\n";
        for (int i = 0; i < (int)GENRE_DISPLAY.size(); i++)
            cout << "  " << (i+1) << ". " << GENRE_DISPLAY[i].second << "\n";
        cout << "Enter genre name or number: ";
        string genreInput;
        getline(cin, genreInput);

        try {
            int idx = stoi(genreInput);
            if (idx >= 1 && idx <= (int)GENRE_DISPLAY.size())
                searchGenre = GENRE_DISPLAY[idx-1].first;
        } catch (...) {
            searchGenre = genreInput;
            transform(searchGenre.begin(), searchGenre.end(), searchGenre.begin(), ::tolower);
            searchGenre.erase(remove(searchGenre.begin(), searchGenre.end(), ' '), searchGenre.end());
        }

        candidateIds = getMoviesByPrimaryGenre(searchGenre, gmap);
        if (candidateIds.size() < 20)
        {
            auto expanded = getMoviesByGenreThreshold(searchGenre, gmap, 0.15);
            unordered_set<int> already(candidateIds.begin(), candidateIds.end());
            for (int id : expanded)
                if (!already.count(id)) candidateIds.push_back(id);
        }
        cout << "Found " << candidateIds.size() << " movies in genre.\n";
    }
    else if (searchMode == 2)
    {
        cout << "Enter actor name: ";
        string actorInput;
        getline(cin, actorInput);
        candidateIds = getMoviesByActor(actorInput, "movies_synchronized.csv");
        cout << "Found " << candidateIds.size() << " movies with that actor.\n";
        if (candidateIds.empty())
            cout << "  Tip: use full name e.g. \"Leonardo DiCaprio\"\n";
    }
    else if (searchMode == 3)
    {
        cout << "Enter director name: ";
        string dirInput;
        getline(cin, dirInput);
        candidateIds = getMoviesByDirector(dirInput, "movies_synchronized.csv");
        cout << "Found " << candidateIds.size() << " movies by that director.\n";
        if (candidateIds.empty())
            cout << "  Tip: use full name e.g. \"Christopher Nolan\"\n";
    }
    else if (searchMode == 4)
    {
        cout << "Describe what you are looking for (plot/keywords): ";
        string plotInput;
        getline(cin, plotInput);
        candidateIds = getMoviesByPlot(plotInput, "movies_synchronized.csv");
        cout << "Found " << candidateIds.size() << " matching movies.\n";
    }

    // ── Build query vector from user's text description ───────────────────────
    cout << "Any other craving or mood? (press Enter to skip): ";
    string query;
    getline(cin, query);

    vector<vector<string>> docs;
    for (auto &mv : movieVectors)
    {
        vector<string> words;
        for (auto &it : mv.second) words.push_back(it.first);
        docs.push_back(words);
    }
    auto df            = computeDF(docs);
    Vector queryVector = buildQueryVector(query, df, movieVectors.size());
    auto queryTopics   = processQuery(query);

    // ── User profile ──────────────────────────────────────────────────────────
    Vector userVector = buildUserVector(ratingsFile, movieVectors, userId);

    // ── Cold start ────────────────────────────────────────────────────────────
    if (rated.empty())
    {
        cout << "\nHello new user! Showing content-based recommendations.\n";
        vector<pair<int,Vector>> candidates;
        if (!candidateIds.empty())
        {
            unordered_set<int> cidSet(candidateIds.begin(), candidateIds.end());
            for (auto &mv : movieVectors)
                if (cidSet.count(mv.first)) candidates.push_back(mv);
        }
        else candidates = movieVectors;

        auto recs = coldStartRecommend(candidates, queryVector, 10);
        cout << "\nRecommended Movies:\n";
        for (int id : recs)
            cout << "  " << titles[id] << "\n";
        return;
    }

    // ── Load CF + MF + GP scores ──────────────────────────────────────────────
    cout << "Computing personalized recommendations...\n";

    RatingsMap allRatings = loadAllRatings("ratings_processed.csv");

    unordered_map<int, double> cfScores  = getCFScores(userId, allRatings, rated);

    MFModel mfModel = loadMFModel("mf_model.bin");
    unordered_map<int, double> mfScores  = getMFScores(userId, mfModel, rated);

    // GP: Graph Propagation over the user-movie bipartite graph.
    // Reuses allRatings loaded above — no extra I/O cost.
    // Returns per-movie propagated score, already normalised to [0,1].
    //
    // We cast RatingsMap → GPRatingsMap; both are the same underlying type
    // (unordered_map<int, unordered_map<int, double>>) so this is zero-cost.
    unordered_map<int, double> gpScores = computeGP(
        reinterpret_cast<const GPRatingsMap &>(allRatings),
        userId,
        rated);

    // ── Hybrid scoring weights (adaptive by rating count) ─────────────────────
    int    ratingCount = (int)rated.size();

    double wCBF, wQ, wT, wCF, wMF, wGP;

    if (ratingCount >= 20)
    {
        wCBF = 0.28; wQ = 0.12; wT = 0.06;
        wCF  = 0.18; wMF = 0.18; wGP = 0.18;
    }
    else if (ratingCount >= 5)
    {
        wCBF = 0.33; wQ = 0.15; wT = 0.08;
        wCF  = 0.10; wMF = 0.10; wGP = 0.12;
    }
    else
    {
        wCBF = 0.33; wQ = 0.15; wT = 0.08;
        wCF  = 0.03; wMF = 0.03; wGP = 0.06;
    }

    double wG = searchGenre.empty() ? 0.0 : 0.08;

    double wSum = wCBF + wQ + wT + wCF + wMF + wGP + wG;
    wCBF /= wSum; wQ   /= wSum; wT   /= wSum;
    wCF  /= wSum; wMF  /= wSum; wGP  /= wSum; wG /= wSum;

    // ── Build candidate pool and score every movie ────────────────────────────
    unordered_set<int> cidSet(candidateIds.begin(), candidateIds.end());
    bool useFilter = !candidateIds.empty();

    vector<pair<double, int>> scores;

    for (auto &mv : movieVectors)
    {
        int movieId = mv.first;

        if (rated.count(movieId)) continue;
        if (useFilter && !cidSet.count(movieId)) continue;

        double cbfScore   = cosineSimilarity(userVector, mv.second);
        double queryScore = query.empty() ? 0.0
                          : cosineSimilarity(queryVector, mv.second);

        double topicScore = 0.0;
        if (movieTopicsMap.count(movieId))
            topicScore = topicSimilarity(queryTopics, movieTopicsMap.at(movieId));

        double cfScore  = cfScores.count(movieId) ? cfScores.at(movieId) : 0.0;
        double mfScore  = mfScores.count(movieId) ? mfScores.at(movieId) : 0.0;

        // GP: propagated graph score, normalised [0,1]
        double gpScore = gpScores.count(movieId) ? gpScores.at(movieId) : 0.0;

        double genreScore = searchGenre.empty() ? 0.0
                          : getGenreWeight(movieId, searchGenre, gmap);

        double finalScore = wCBF * cbfScore
                          + wQ   * queryScore
                          + wT   * topicScore
                          + wCF  * cfScore
                          + wMF  * mfScore
                          + wGP  * gpScore
                          + wG   * genreScore;

        scores.push_back({finalScore, movieId});
    }

    sort(scores.rbegin(), scores.rend());

    // ── Output ────────────────────────────────────────────────────────────────
    cout << "\nRecommended Movies:\n";
    int shown = 0;
    for (auto &s : scores)
    {
        if (shown >= 10) break;
        string genreTag;
        auto git = gmap.find(s.second);
        if (git != gmap.end())
            genreTag = " [" + git->second.primaryGenre + "]";
        cout << "  " << titles[s.second] << genreTag << "\n";
        shown++;
    }
}
