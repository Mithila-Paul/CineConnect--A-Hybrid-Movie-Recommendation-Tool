#include "User.h"
#include "cbf.h"
#include "tf-idf.h"
#include "cf.h"
#include "genre.h"
#include "mf.h"
#include "gp.h"
#include "trends.h"
#include "csv_parser.h"
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

static RatingsMap &getSessionRatings()
{
    static RatingsMap cache;
    static bool loaded = false;

    if (!loaded)
    {
        cache = loadAllRatings("ratings_processed.csv");
        loaded = true;
    }

    return cache;
}

static void markRatingAdded(int userId, int movieId, double rating)
{
    getSessionRatings()[userId][movieId] = rating;
}

static const MFModel &getSessionMFModel()
{
    static MFModel model;
    static bool loaded = false;

    if (!loaded)
    {
        model = loadMFModel("mf_model.bin");
        loaded = true;
    }

    return model;
}

static string toLower(const string &s)
{
    string r = s;

    for (int i = 0; i < (int)r.size(); i++)
    {
        r[i] = (char)tolower((unsigned char)r[i]);
    }

    return r;
}

static int editDistance(const string &a, const string &b)
{
    int m = (int)a.size();
    int n = (int)b.size();

    vector<vector<int>> dp(m + 1, vector<int>(n + 1, 0));

    for (int i = 0; i <= m; i++)
    {
        dp[i][0] = i;
    }

    for (int j = 0; j <= n; j++)
    {
        dp[0][j] = j;
    }

    for (int i = 1; i <= m; i++)
    {
        for (int j = 1; j <= n; j++)
        {
            if (a[i - 1] == b[j - 1])
            {
                dp[i][j] = dp[i - 1][j - 1];
            }
            else
            {
                int best = dp[i - 1][j];
                if (dp[i][j - 1] < best)
                {
                    best = dp[i][j - 1];
                }
                if (dp[i - 1][j - 1] < best)
                {
                    best = dp[i - 1][j - 1];
                }

                dp[i][j] = 1 + best;
            }
        }
    }

    return dp[m][n];
}

struct MovieRecord
{
    string title;
    string tags;
};

static unordered_map<int, MovieRecord> loadMovieCatalog(const string &csvFile = "movies_synchronized.csv")
{
    unordered_map<int, MovieRecord> catalog;

    ifstream file(csvFile);
    if (!file.is_open())
    {
        return catalog;
    }

    string line;
    getline(file, line);

    while (getline(file, line))
    {
        stripCR(line);

        if (line.empty())
        {
            continue;
        }

        istringstream ss(line);
        string idStr = parseCsvField(ss);
        string title = parseCsvField(ss);
        string tags;

        getline(ss, tags);

        if (!tags.empty() && tags[0] == '"')
        {
            tags = tags.substr(1);
        }

        if (!tags.empty() && tags[tags.size() - 1] == '"')
        {
            tags.erase(tags.size() - 1);
        }

        try
        {
            MovieRecord rec;
            rec.title = title;
            rec.tags = tags;
            catalog[stoi(idStr)] = rec;
        }
        catch (...)
        {
        }
    }

    file.close();
    return catalog;
}

static const unordered_map<int, MovieRecord> &getCatalog()
{
    static unordered_map<int, MovieRecord> cache;
    static bool loaded = false;

    if (!loaded)
    {
        cache = loadMovieCatalog("movies_synchronized.csv");
        loaded = true;
    }

    return cache;
}

static bool isQuitInput(const string &s)
{
    string t = toLower(s);

    if (t == "q" || t == "quit" || t == "back")
    {
        return true;
    }

    return false;
}

static void clearInputLine()
{
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

static void markRatingRemoved(int userId, int movieId)
{
    RatingsMap &ratings = getSessionRatings();
    RatingsMap::iterator uit = ratings.find(userId);

    if (uit == ratings.end())
    {
        return;
    }

    uit->second.erase(movieId);

    if (uit->second.empty())
    {
        ratings.erase(uit);
    }
}

static void markUserDeleted(int userId)
{
    getSessionRatings().erase(userId);
}

static vector<string> extractUniqueTailNames(bool directorMode)
{
    vector<string> names;
    unordered_set<string> seen;
    const unordered_map<int, MovieRecord> &catalog = getCatalog();

    unordered_map<int, MovieRecord>::const_iterator it;
    for (it = catalog.begin(); it != catalog.end(); ++it)
    {
        string tags = toLower(it->second.tags);
        stringstream ss(tags);
        vector<string> tokens;
        string tok;

        while (ss >> tok)
        {
            tok.erase(
                remove_if(
                    tok.begin(),
                    tok.end(),
                    [](unsigned char c)
                    {
                        return ispunct(c);
                    }),
                tok.end());

            if (!tok.empty())
            {
                tokens.push_back(tok);
            }
        }

        if (tokens.empty())
        {
            continue;
        }

        if (directorMode)
        {
            string name = tokens[tokens.size() - 1];

            if (!seen.count(name))
            {
                seen.insert(name);
                names.push_back(name);
            }
        }
        else
        {
            int start = max(0, (int)tokens.size() - 4);
            int end = (int)tokens.size() - 1;

            for (int i = start; i < end; i++)
            {
                string name = tokens[i];

                if (!seen.count(name))
                {
                    seen.insert(name);
                    names.push_back(name);
                }
            }
        }
    }

    return names;
}

static string prettifyTokenName(const string &token)
{
    if (token.empty())
    {
        return token;
    }

    string s = token;
    s[0] = (char)toupper((unsigned char)s[0]);

    return s;
}

static bool compareCloseMatches(const pair<int, string> &a, const pair<int, string> &b)
{
    return a.first < b.first;
}

static bool compareSubstringMatches(const pair<int, string> &a, const pair<int, string> &b)
{
    return a.second.size() < b.second.size();
}

static bool compareFuzzyMatches(
    const pair<int, pair<int, string>> &a,
    const pair<int, pair<int, string>> &b)
{
    return a.first < b.first;
}

static bool resolvePersonNameWithRetry(
    const string &label,
    bool directorMode,
    string &resolvedName,
    vector<int> &candidateIds)
{
    vector<string> allNames = extractUniqueTailNames(directorMode);

    for (int attempt = 1; attempt <= 2; attempt++)
    {
        cout << "Enter " << label << " name (or q to go back): ";

        string input;
        getline(cin, input);

        if (isQuitInput(input))
        {
            return false;
        }

        if (directorMode)
        {
            candidateIds = getMoviesByDirector(input, "movies_synchronized.csv");
        }
        else
        {
            candidateIds = getMoviesByActor(input, "movies_synchronized.csv");
        }

        if (!candidateIds.empty())
        {
            resolvedName = input;
            return true;
        }

        string target = normaliseName(input);
        vector<pair<int, string>> close;

        int maxDist = max(2, (int)target.size() / 3);

        for (int i = 0; i < (int)allNames.size(); i++)
        {
            int dist = editDistance(target, allNames[i]);

            if (dist <= maxDist)
            {
                close.push_back(make_pair(dist, allNames[i]));
            }
        }

        sort(close.begin(), close.end(), compareCloseMatches);

        cout << "No exact " << label << " match found.\n";

        if (!close.empty())
        {
            int show = min(5, (int)close.size());

            cout << "Did you mean:\n";
            for (int i = 0; i < show; i++)
            {
                cout << "  " << (i + 1) << ". " << prettifyTokenName(close[i].second) << "\n";
            }

            cout << "Choose suggestion number, or press 0 to retry: ";

            int pick;
            cin >> pick;

            if (cin.fail())
            {
                clearInputLine();
                pick = 0;
            }
            else
            {
                clearInputLine();
            }

            if (pick >= 1 && pick <= show)
            {
                resolvedName = close[pick - 1].second;

                if (directorMode)
                {
                    candidateIds = getMoviesByDirector(resolvedName, "movies_synchronized.csv");
                }
                else
                {
                    candidateIds = getMoviesByActor(resolvedName, "movies_synchronized.csv");
                }

                if (!candidateIds.empty())
                {
                    return true;
                }
            }
        }

        if (attempt < 2)
        {
            cout << "Try again. Remaining chance: 1\n";
        }
    }

    cout << "Returning to recommendation menu.\n";
    return false;
}

static int findMovieIdByTitle(const string &inputTitle)
{
    const unordered_map<int, MovieRecord> &catalog = getCatalog();
    string target = toLower(inputTitle);

    unordered_map<int, MovieRecord>::const_iterator it;
    for (it = catalog.begin(); it != catalog.end(); ++it)
    {
        if (toLower(it->second.title) == target)
        {
            return it->first;
        }
    }

    vector<pair<int, string>> substringMatches;

    for (it = catalog.begin(); it != catalog.end(); ++it)
    {
        string lower = toLower(it->second.title);

        if (lower.find(target) != string::npos || target.find(lower) != string::npos)
        {
            substringMatches.push_back(make_pair(it->first, it->second.title));
        }
    }

    if (!substringMatches.empty())
    {
        if (substringMatches.size() == 1)
        {
            return substringMatches[0].first;
        }

        sort(substringMatches.begin(), substringMatches.end(), compareSubstringMatches);

        cout << "\nDid you mean one of these?\n";

        int show = min(5, (int)substringMatches.size());
        for (int i = 0; i < show; i++)
        {
            cout << "  " << (i + 1) << ". " << substringMatches[i].second << "\n";
        }

        cout << "Enter number (or 0 to cancel): ";

        int pick;
        cin >> pick;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (pick >= 1 && pick <= show)
        {
            return substringMatches[pick - 1].first;
        }

        return -1;
    }

    int maxDist = max(3, (int)target.size() / 4); // avoid garbage matches for short titles
    vector<pair<int, pair<int, string>>> fuzzyMatches;

    for (it = catalog.begin(); it != catalog.end(); ++it)
    {
        int dist = editDistance(target, toLower(it->second.title));

        if (dist <= maxDist)
        {
            fuzzyMatches.push_back(make_pair(dist, make_pair(it->first, it->second.title)));
        }
    }

    if (fuzzyMatches.empty())
    {
        return -1;
    }

    sort(fuzzyMatches.begin(), fuzzyMatches.end(), compareFuzzyMatches);

    cout << "\nMovie not found. Did you mean:\n";

    int show = min(5, (int)fuzzyMatches.size());
    for (int i = 0; i < show; i++)
    {
        cout << "  " << (i + 1) << ". " << fuzzyMatches[i].second.second << "\n";
    }

    cout << "Enter number (or 0 to cancel): ";

    int pick;
    cin >> pick;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    if (pick >= 1 && pick <= show)
    {
        return fuzzyMatches[pick - 1].second.first;
    }

    return -1;
}

static string getMovieTitleById(int movieId)
{
    const unordered_map<int, MovieRecord> &catalog = getCatalog();
    unordered_map<int, MovieRecord>::const_iterator it = catalog.find(movieId);

    if (it != catalog.end())
    {
        return it->second.title;
    }

    return "Unknown Title";
}

static string getMiniPlot(
    int movieId,
    const unordered_map<int, MovieRecord> &catalog)
{
    unordered_map<int, MovieRecord>::const_iterator it = catalog.find(movieId);

    if (it == catalog.end())
    {
        return "";
    }

    string tags = it->second.tags;

    if ((int)tags.size() > 130)
    {
        size_t cut = tags.find(". ", 60);

        if (cut == string::npos || cut > 160)
        {
            cut = 130;

            size_t lastSpace = tags.rfind(' ', cut);
            if (lastSpace != string::npos && lastSpace > 80)
            {
                cut = lastSpace;
            }

            tags = tags.substr(0, cut) + "...";
        }
        else
        {
            tags = tags.substr(0, cut + 1);
        }
    }

    bool capNext = true;

    for (int i = 0; i < (int)tags.size(); i++)
    {
        if (capNext && isalpha((unsigned char)tags[i]))
        {
            tags[i] = (char)toupper((unsigned char)tags[i]);
            capNext = false;
        }

        if (i + 1 < (int)tags.size() &&
            (tags[i] == '.' || tags[i] == '!') &&
            tags[i + 1] == ' ')
        {
            capNext = true;
        }
    }

    return tags;
}

static int generateUserId()
{
    for (int id = 611;; id++)
    {
        ifstream fin("user" + to_string(id) + "_ratings.txt");

        if (!fin)
        {
            return id;
        }
    }
}

bool hasRatedMovie(int userId, int movieId)
{
    ifstream fin("user" + to_string(userId) + "_ratings.txt");

    if (fin.is_open())
    {
        string line;

        while (getline(fin, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
            {
                line.erase(line.size() - 1);
            }

            stringstream ss(line);
            string midStr, title, rStr;

            getline(ss, midStr, '|');
            getline(ss, title, '|');
            getline(ss, rStr, '|');

            midStr.erase(remove_if(midStr.begin(), midStr.end(), ::isspace), midStr.end());

            if (!midStr.empty())
            {
                try
                {
                    if (stoi(midStr) == movieId)
                    {
                        fin.close();
                        return true;
                    }
                }
                catch (...)
                {
                }
            }
        }

        fin.close();
    }

    if (userId >= 1 && userId <= 610)
    {
        ifstream global("ratings_processed.csv");

        if (global.is_open())
        {
            string line;
            getline(global, line);

            while (getline(global, line))
            {
                if (!line.empty() && line[line.size() - 1] == '\r')
                {
                    line.erase(line.size() - 1);
                }

                stringstream ss(line);
                string mStr, uStr;

                getline(ss, mStr, ',');
                getline(ss, uStr, ',');

                try
                {
                    if (stoi(uStr) == userId && stoi(mStr) == movieId)
                    {
                        global.close();
                        return true;
                    }
                }
                catch (...)
                {
                }
            }

            global.close();
        }
    }

    return false;
}

void appendToGlobalRatings(int userId, int movieId, double rating)
{
    ofstream fout("ratings_processed.csv", ios::app);
    fout << movieId << "," << userId << "," << rating << "\n";
    fout.close();

    markRatingAdded(userId, movieId, rating);
}

void removeFromGlobalRatings(int userId, int movieId)
{
    ifstream fin("ratings_processed.csv");
    ofstream tmp("ratings_processed_tmp.csv");

    if (!fin.is_open() || !tmp.is_open())
    {
        return;
    }

    string line;
    getline(fin, line);

    if (!line.empty() && line[line.size() - 1] == '\r')
    {
        line.erase(line.size() - 1);
    }

    tmp << line << "\n";

    while (getline(fin, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }

        if (line.empty())
        {
            continue;
        }

        stringstream ss(line);
        string mStr, uStr, rStr;

        getline(ss, mStr, ',');
        getline(ss, uStr, ',');
        getline(ss, rStr, ',');

        bool isTarget = false;

        try
        {
            isTarget = (stoi(mStr) == movieId && stoi(uStr) == userId);
        }
        catch (...)
        {
        }

        if (!isTarget)
        {
            tmp << line << "\n";
        }
    }

    fin.close();
    tmp.close();

    remove("ratings_processed.csv");
    rename("ratings_processed_tmp.csv", "ratings_processed.csv");

    markRatingRemoved(userId, movieId);
}

void removeFromUserFile(int userId, int movieId)
{
    string filename = "user" + to_string(userId) + "_ratings.txt";

    ifstream fin(filename);
    ofstream tmp("user_tmp.txt");

    if (!fin.is_open() || !tmp.is_open())
    {
        return;
    }

    string line;

    while (getline(fin, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }

        stringstream ss(line);
        string midStr, title, rStr;

        getline(ss, midStr, '|');
        getline(ss, title, '|');
        getline(ss, rStr, '|');

        midStr.erase(remove_if(midStr.begin(), midStr.end(), ::isspace), midStr.end());

        if (midStr.empty())
        {
            continue;
        }

        try
        {
            if (stoi(midStr) == movieId)
            {
                continue;
            }
        }
        catch (...)
        {
            continue;
        }

        tmp << line << "\n";
    }

    fin.close();
    tmp.close();

    remove(filename.c_str());
    rename("user_tmp.txt", filename.c_str());
}

void rateMovie(int userId)
{
    string title;
    double rating;

    cout << "Enter Movie Title (or q to go back): ";
    getline(cin, title);

    if (isQuitInput(title))
    {
        return;
    }

    int movieId = findMovieIdByTitle(title);

    if (movieId == -1)
    {
        cout << "Movie not found!\n";
        return;
    }

    string movieTitle = getMovieTitleById(movieId);

    if (hasRatedMovie(userId, movieId))
    {
        cout << "You already rated \"" << movieTitle << "\".\n";
        cout << "Do you want to replace your rating? (y/n): ";

        char choice;
        cin >> choice;

        if (choice != 'y' && choice != 'Y')
        {
            return;
        }

        removeFromUserFile(userId, movieId);
        removeFromGlobalRatings(userId, movieId);

        cout << "Old rating removed.\n";
    }

    cout << "Enter Rating (1 to 5): ";
    cin >> rating;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (rating < 1 || rating > 5)
    {
        cout << "Invalid rating! Must be between 1 and 5.\n";
        return;
    }

    
    ofstream fout("user" + to_string(userId) + "_ratings.txt", ios::app);
    fout << movieId << " | " << movieTitle << " | " << rating << "\n";
    fout.close();

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
    unordered_set<int> seen;

    while (getline(fin, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }

        if (line.empty())
        {
            continue;
        }

        stringstream ss(line);
        string midStr, title, rStr;

        getline(ss, midStr, '|');
        getline(ss, title, '|');
        getline(ss, rStr, '|');

        midStr.erase(remove_if(midStr.begin(), midStr.end(), ::isspace), midStr.end());
        rStr.erase(remove_if(rStr.begin(), rStr.end(), ::isspace), rStr.end());

        if (!title.empty() && title[0] == ' ')
        {
            title.erase(0, 1);
        }

        if (!title.empty() && title[title.size() - 1] == ' ')
        {
            title.erase(title.size() - 1);
        }

        if (midStr.empty() || rStr.empty())
        {
            continue;
        }

        try
        {
            int mid = stoi(midStr);

            if (seen.count(mid))
            {
                continue;
            }

            seen.insert(mid);
            cout << "  " << title << " (" << mid << ") : " << stod(rStr) << "/5\n";
        }
        catch (...)
        {
        }
    }

    fin.close();
}

static vector<int> rankCandidatesByTrends(
    const vector<int> &candidateIds,
    const TrendsData &trends,
    int topN = 10)
{
    vector<int> ranked;

    if (candidateIds.empty() || !trends.ready)
    {
        return ranked;
    }

    unordered_set<int> wanted(candidateIds.begin(), candidateIds.end());

    for (int i = 0; i < (int)trends.topMovies.size(); i++)
    {
        if ((int)ranked.size() >= topN)
        {
            break;
        }

        if (wanted.count(trends.topMovies[i].movieId))
        {
            ranked.push_back(trends.topMovies[i].movieId);
        }
    }

    if ((int)ranked.size() < topN)
    {
        unordered_set<int> already(ranked.begin(), ranked.end());

        for (int i = 0; i < (int)candidateIds.size(); i++)
        {
            if ((int)ranked.size() >= topN)
            {
                break;
            }

            if (!already.count(candidateIds[i]))
            {
                ranked.push_back(candidateIds[i]);
            }
        }
    }

    return ranked;
}

void recommendMoviesForUser(int userId, const TrendsData &trends)
{
    vector<pair<int, Vector>> movieVectors = loadMovieVectors("movie_vectors.txt");
    unordered_map<int, string> titles = loadMovieTitles("movies_synchronized.csv");
    unordered_map<int, vector<string>> movieTopicsMap = loadMovieTopics("movie_topics.txt");
    GenreMap gmap = loadGenreMap("movie_genres.txt");
    const unordered_map<int, MovieRecord> &catalog = getCatalog();

    string ratingsFile = "user" + to_string(userId) + "_ratings.txt";
    unordered_set<int> rated = getRatedMovieIds(ratingsFile, userId);

    cout << "\nHow do you want to search?\n";
    cout << "  1. By genre\n";
    cout << "  2. By actor\n";
    cout << "  3. By director\n";
    cout << "  4. By plot / keywords\n";
    cout << "  5. Random Query\n";
    cout << "Enter choice: ";

    int searchMode;
    cin >> searchMode;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(10000, '\n');
        searchMode = 5;
    }

    cin.ignore(10000, '\n');

    vector<int> candidateIds;
    string searchGenre;

    if (searchMode == 1)
    {
        bool resolved = false;

        for (int attempt = 1; attempt <= 2 && !resolved; attempt++)
        {
            cout << "\nAvailable genres:\n";

            for (int i = 0; i < (int)GENRE_DISPLAY.size(); i++)
            {
                cout << "  " << (i + 1) << ". " << GENRE_DISPLAY[i].second << "\n";
            }

            cout << "Enter genre name or number (or q to go back): ";

            string genreInput;
            getline(cin, genreInput);

            if (isQuitInput(genreInput))
            {
                cout << "Returning to menu.\n";
                return;
            }

            string normalized;
            bool valid = false;

            try
            {
                int idx = stoi(genreInput);

                if (idx >= 1 && idx <= (int)GENRE_DISPLAY.size())
                {
                    normalized = GENRE_DISPLAY[idx - 1].first;
                    valid = true;
                }
            }
            catch (...)
            {
            }

            if (!valid)
            {
                normalized = toLower(genreInput);
                normalized.erase(remove(normalized.begin(), normalized.end(), ' '), normalized.end());

                for (int i = 0; i < (int)GENRE_DISPLAY.size(); i++)
                {
                    if (GENRE_DISPLAY[i].first == normalized)
                    {
                        valid = true;
                        break;
                    }
                }
            }

            if (!valid)
            {
                cout << "Invalid genre input.";

                if (attempt < 2)
                {
                    cout << " Try once more.\n";
                }
                else
                {
                    cout << " Returning to menu.\n";
                }

                continue;
            }

            searchGenre = normalized;
            candidateIds = getMoviesByPrimaryGenre(searchGenre, gmap);

            if ((int)candidateIds.size() < 20)
            {
                vector<int> expanded = getMoviesByGenreThreshold(searchGenre, gmap, 0.15);
                unordered_set<int> already(candidateIds.begin(), candidateIds.end());

                for (int i = 0; i < (int)expanded.size(); i++)
                {
                    if (!already.count(expanded[i]))
                    {
                        candidateIds.push_back(expanded[i]);
                    }
                }
            }

            cout << "Found " << candidateIds.size() << " movies in genre.\n";
            resolved = true;
        }

        if (searchGenre.empty())
        {
            return;
        }
    }
    else if (searchMode == 2)
    {
        string actorName;

        if (!resolvePersonNameWithRetry("actor", false, actorName, candidateIds))
        {
            return;
        }

        cout << "Found " << candidateIds.size() << " movies with actor \"" << actorName << "\".\n";
        candidateIds = rankCandidatesByTrends(candidateIds, trends, 50);
    }
    else if (searchMode == 3)
    {
        string directorName;

        if (!resolvePersonNameWithRetry("director", true, directorName, candidateIds))
        {
            return;
        }

        cout << "Found " << candidateIds.size() << " movies by director \"" << directorName << "\".\n";
        candidateIds = rankCandidatesByTrends(candidateIds, trends, 50);
    }
    else if (searchMode == 4)
    {
        cout << "Describe what you are looking for (plot/keywords, or q to go back): ";

        string plotInput;
        getline(cin, plotInput);

        if (isQuitInput(plotInput))
        {
            return;
        }

        candidateIds = getMoviesByPlot(plotInput, "movies_synchronized.csv");
        candidateIds = rankCandidatesByTrends(candidateIds, trends, 50);
        if (candidateIds.empty())
        {
            cout << "  No movies matched your keywords.\n";
            cout << "  Try fewer or more general words (e.g. \"revenge\" instead of \"seeks revenge\").\n";
        }
        else
        {
            cout << "Found " << candidateIds.size() << " matching movies"
                 << " (ranked by keyword overlap).\n";
        }
    }

    cout << "Any other query to descibe your mood? (press Enter to skip): ";

    string query;
    getline(cin, query);

    const unordered_map<string, int> &df = getGlobalDF();
    int totalDocs = getGlobalTotalDocs();
    Vector queryVector = buildQueryVector(query, df, totalDocs);
    vector<string> queryTopics = processQuery(query);

    Vector userVector = buildUserVector(ratingsFile, movieVectors, userId);

    if (rated.empty())
    {
        cout << "\nHello new user! Showing starter recommendations.\n";

        if (query.empty())
        {
            vector<int> fallbackIds;

            if (!searchGenre.empty())
            {
                fallbackIds = getTrendingMoviesByGenre(searchGenre, trends, 10);
            }
            else if (!candidateIds.empty())
            {
                unordered_set<int> candSet(candidateIds.begin(), candidateIds.end());

                for (int i = 0; i < (int)trends.topMovies.size(); i++)
                {
                    if ((int)fallbackIds.size() >= 10)
                    {
                        break;
                    }

                    if (candSet.count(trends.topMovies[i].movieId))
                    {
                        fallbackIds.push_back(trends.topMovies[i].movieId);
                    }
                }
            }
            else
            {
                for (int i = 0; i < (int)trends.topMovies.size(); i++)
                {
                    if ((int)fallbackIds.size() >= 10)
                    {
                        break;
                    }

                    fallbackIds.push_back(trends.topMovies[i].movieId);
                }
            }

            cout << "\n--- TOP PICKS FOR YOU ---\n";

            int shown = 0;

            for (int i = 0; i < (int)fallbackIds.size(); i++)
            {
                int id = fallbackIds[i];
                string titleStr = titles.count(id) ? titles[id] : "Unknown";
                string genreTag;

                GenreMap::iterator git = gmap.find(id);
                if (git != gmap.end())
                {
                    genreTag = getGenreDisplayName(git->second.primaryGenre);
                }

                string plot = getMiniPlot(id, catalog);

                cout << "\n  #" << (shown + 1) << "  " << titleStr << "\n";

                if (!genreTag.empty())
                {
                    cout << "       Genre : " << genreTag << "\n";
                }

                if (!plot.empty())
                {
                    cout << "       Plot  : " << plot << "\n";
                }

                shown++;

                if (shown >= 10)
                {
                    break;
                }
            }

            cout << "\n-------------------------\n";
            return;
        }

        vector<string> priorGenres;

        if (!searchGenre.empty())
        {
            priorGenres.push_back(searchGenre);
        }

        vector<pair<int, Vector>> candidates;
        unordered_set<int> cidSet(candidateIds.begin(), candidateIds.end());

        if (!candidateIds.empty())
        {
            for (int i = 0; i < (int)movieVectors.size(); i++)
            {
                if (cidSet.count(movieVectors[i].first))
                {
                    candidates.push_back(movieVectors[i]);
                }
            }
        }
        else
        {
            candidates = movieVectors;
        }

        double wQuery = query.empty() ? 0.0 : 0.6;
        double wGenre = priorGenres.empty() ? 0.0 : 0.4;

        if (wQuery + wGenre == 0.0)
        {
            wQuery = 1.0;
        }

        vector<pair<double, int>> coldScores;
        coldScores.reserve(candidates.size());

        for (int i = 0; i < (int)candidates.size(); i++)
        {
            int id = candidates[i].first;
            double qScore = (wQuery > 0.0) ? cosineSimilarity(queryVector, candidates[i].second) : 0.0;
            double gScore = 0.0;

            if (wGenre > 0.0 && !priorGenres.empty())
            {
                for (int j = 0; j < (int)priorGenres.size(); j++)
                {
                    gScore += getGenreWeight(id, priorGenres[j], gmap);
                }

                gScore /= (double)priorGenres.size();
            }

            coldScores.push_back(make_pair(wQuery * qScore + wGenre * gScore, id));
        }

        sort(coldScores.rbegin(), coldScores.rend());

        cout << "\n--- TOP PICKS FOR YOU ---\n";

        int shown = 0;

        for (int i = 0; i < (int)coldScores.size(); i++)
        {
            if (shown >= 10)
            {
                break;
            }

            int id = coldScores[i].second;
            string titleStr = titles.count(id) ? titles[id] : "Unknown";
            string genreTag;

            GenreMap::iterator git = gmap.find(id);
            if (git != gmap.end())
            {
                genreTag = getGenreDisplayName(git->second.primaryGenre);
            }

            string plot = getMiniPlot(id, catalog);

            cout << "\n  #" << (shown + 1) << "  " << titleStr << "\n";

            if (!genreTag.empty())
            {
                cout << "       Genre : " << genreTag << "\n";
            }

            if (!plot.empty())
            {
                cout << "       Plot  : " << plot << "\n";
            }

            shown++;
        }

        cout << "\n-------------------------\n";
        return;
    }

    cout << "Computing personalized recommendations...\n";

    const RatingsMap &allRatings = getSessionRatings();
    unordered_map<int, double> cfScores = getCFScores(userId, allRatings, rated);

    const MFModel &mfModel = getSessionMFModel();
    unordered_map<int, double> mfScores = getMFScores(userId, mfModel, rated);

    unordered_map<int, double> gpScores = computeGP(allRatings, userId, rated);

    int ratingCount = (int)rated.size();

    double wCBF, wQ, wT, wCF, wMF, wGP;

    if (ratingCount >= 20)
    {
        wCBF = 0.28;
        wQ = 0.12;
        wT = 0.06;
        wCF = 0.18;
        wMF = 0.18;
        wGP = 0.18;
    }
    else if (ratingCount >= 5)
    {
        wCBF = 0.33;
        wQ = 0.15;
        wT = 0.08;
        wCF = 0.10;
        wMF = 0.10;
        wGP = 0.12;
    }
    else
    {
        wCBF = 0.33;
        wQ = 0.15;
        wT = 0.08;
        wCF = 0.03;
        wMF = 0.03;
        wGP = 0.06;
    }

    double wG = searchGenre.empty() ? 0.0 : 0.08;

    double wSum = wCBF + wQ + wT + wCF + wMF + wGP + wG;

    wCBF /= wSum;
    wQ /= wSum;
    wT /= wSum;
    wCF /= wSum;
    wMF /= wSum;
    wGP /= wSum;
    wG /= wSum;

    unordered_set<int> cidSet(candidateIds.begin(), candidateIds.end());
    bool useFilter = !candidateIds.empty();

    struct ScoreEntry
    {
        double final;
        int movieId;
        double cbf;
        double cf;
        double mf;
        double gp;
    };

    vector<ScoreEntry> scores;
    scores.reserve(movieVectors.size());

    for (int i = 0; i < (int)movieVectors.size(); i++)
    {
        int movieId = movieVectors[i].first;

        if (rated.count(movieId))
        {
            continue;
        }

        if (useFilter && !cidSet.count(movieId))
        {
            continue;
        }

        double cbfScore = cosineSimilarity(userVector, movieVectors[i].second);
        double queryScore = query.empty() ? 0.0 : cosineSimilarity(queryVector, movieVectors[i].second);
        double topicScore = 0.0;
        if (movieTopicsMap.count(movieId))
        {
            topicScore = topicSimilarity(queryTopics, movieTopicsMap.at(movieId));
        }

        double cfScore = cfScores.count(movieId) ? cfScores.at(movieId) : 0.0;
        double mfScore = mfScores.count(movieId) ? mfScores.at(movieId) : 0.0;
        double gpScore = gpScores.count(movieId) ? gpScores.at(movieId) : 0.0;

        double genreScore = searchGenre.empty() ? 0.0
                                                : getGenreWeight(movieId, searchGenre, gmap);

        

        double finalScore =
            wCBF * cbfScore +
            wQ * queryScore +
            wT * topicScore +
            wCF * cfScore +
            wMF * mfScore +
            wGP * gpScore +
            wG * genreScore;

        ScoreEntry entry;
        entry.final = finalScore;
        entry.movieId = movieId;
        entry.cbf = wCBF * cbfScore;
        entry.cf = wCF * cfScore;
        entry.mf = wMF * mfScore;
        entry.gp = wGP * gpScore;

        scores.push_back(entry);
    }

    sort(
        scores.begin(),
        scores.end(),
        [](const ScoreEntry &a, const ScoreEntry &b)
        {
            return a.final > b.final;
        });

    cout << "\n--- YOUR PERSONALIZED TOP PICKS ---\n";

    int shown = 0;

    for (int i = 0; i < (int)scores.size(); i++)
    {
        ScoreEntry &s = scores[i];

        if (shown >= 10)
        {
            break;
        }

        int movieId = s.movieId;
        string titleStr = titles.count(movieId) ? titles[movieId] : "Unknown";
        string genreTag;

        GenreMap::iterator git = gmap.find(movieId);
        if (git != gmap.end())
        {
            genreTag = getGenreDisplayName(git->second.primaryGenre);
        }

        string plot = getMiniPlot(movieId, catalog);

        double totalPct = (s.final > 0.0) ? (s.final * 100.0) : 0.0;
        double cbfPct = 0.0;
        double cfPct = 0.0;
        double mfPct = 0.0;
        double gpPct = 0.0;
        double otherPct = 0.0;

        if (s.final > 0.0)
        {
            cbfPct = (s.cbf / s.final) * 100.0;
            cfPct = (s.cf / s.final) * 100.0;
            mfPct = (s.mf / s.final) * 100.0;
            gpPct = (s.gp / s.final) * 100.0;

            otherPct = 100.0 - cbfPct - cfPct - mfPct - gpPct;

            if (otherPct < 0.0)
            {
                otherPct = 0.0;
            }
        }

        cout << fixed << setprecision(1);

        cout << "\n  #" << (shown + 1) << "  " << titleStr << "\n";

        if (!genreTag.empty())
        {
            cout << "       Genre : " << genreTag << "\n";
        }

        cout << "       Match : " << totalPct << "%"
             << "  (CBF " << cbfPct << "%"
             << "  CF " << cfPct << "%"
             << "  MF " << mfPct << "%"
             << "  GP " << gpPct << "%";

        if (otherPct > 0.05)
        {
            cout << "  Query/Other " << otherPct << "%";
        }

        cout << ")\n";

        if (!plot.empty())
        {
            cout << "       Plot  : " << plot << "\n";
        }

        shown++;
    }

    cout << "\n-----------------------------------\n";
}

string getGenreDisplayName(const string &genreKey)
{
    for (int i = 0; i < (int)GENRE_DISPLAY.size(); i++)
    {
        if (GENRE_DISPLAY[i].first == genreKey)
        {
            return GENRE_DISPLAY[i].second;
        }
    }

    return genreKey;
}

vector<string> chooseFavoriteGenres(int maxGenres, const TrendsData *trends)
{
    vector<string> chosen;
    unordered_set<string> used;
    unordered_map<string, int> trendRank;

    if (trends && trends->ready)
    {
        for (int i = 0; i < (int)trends->genres.size(); i++)
        {
            trendRank[trends->genres[i].genre] = i + 1;
        }
    }

    cout << "\nChoose up to " << maxGenres << " favorite genres.\n";
    cout << "Enter 0 when finished.\n\n";

    for (int i = 0; i < (int)GENRE_DISPLAY.size(); i++)
    {
        string hint;
        unordered_map<string, int>::iterator it = trendRank.find(GENRE_DISPLAY[i].first);

        if (it != trendRank.end() && it->second <= 3)
        {
            hint = "  [Trending #" + to_string(it->second) + "]";
        }

        cout << setw(2) << (i + 1) << ". " << GENRE_DISPLAY[i].second << hint << "\n";
    }

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

        if (choice == 0)
        {
            break;
        }

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

static void saveSeedRating(int userId, int movieId, double rating)
{
    string movieTitle = getMovieTitleById(movieId);

    ofstream fout("user" + to_string(userId) + "_ratings.txt", ios::app);
    fout << movieId << " | " << movieTitle << " | " << rating << "\n";
    fout.close();

    appendToGlobalRatings(userId, movieId, rating);
}

void runColdStartOnboarding(int userId, const TrendsData &trends)
{
    cout << "\n==============================\n";
    cout << "Cold-Start Onboarding\n";
    cout << "==============================\n";
    cout << "Help us know your taste better.\n";

    if (trends.ready && !trends.genres.empty())
    {
        cout << "\nCurrently trending on CineConnect:\n";

        int show = min(3, (int)trends.genres.size());

        for (int i = 0; i < show; i++)
        {
            cout << "  " << (i + 1) << ". " << trends.genres[i].displayName
                 << "  (avg " << fixed << setprecision(2)
                 << trends.genres[i].avgRating << "/5)\n";
        }

        cout << "\n";
    }

    vector<string> favoriteGenres = chooseFavoriteGenres(3, &trends);

    if (favoriteGenres.empty())
    {
        cout << "No genres selected. Skipping onboarding.\n";
        return;
    }

    unordered_map<int, string> titles = loadMovieTitles("movies_synchronized.csv");
    unordered_set<int> alreadyShown;

    for (int g = 0; g < (int)favoriteGenres.size(); g++)
    {
        string genre = favoriteGenres[g];
        vector<int> seeds;

        if (trends.ready)
        {
            seeds = getTrendingMoviesByGenre(genre, trends, 3);
        }
        else
        {
            GenreMap gmap = loadGenreMap("movie_genres.txt");
            seeds = getMoviesByPrimaryGenre(genre, gmap);

            if ((int)seeds.size() < 3)
            {
                vector<int> expanded = getMoviesByGenreThreshold(genre, gmap, 0.15);
                unordered_set<int> seen(seeds.begin(), seeds.end());

                for (int i = 0; i < (int)expanded.size(); i++)
                {
                    if ((int)seeds.size() >= 3)
                    {
                        break;
                    }

                    if (!seen.count(expanded[i]))
                    {
                        seeds.push_back(expanded[i]);
                        seen.insert(expanded[i]);
                    }
                }
            }

            if ((int)seeds.size() > 3)
            {
                seeds.resize(3);
            }
        }

        if (seeds.empty())
        {
            cout << "\nNo seed movies found for genre: " << getGenreDisplayName(genre) << "\n";
            continue;
        }

        vector<int> uniqueSeeds;

        for (int i = 0; i < (int)seeds.size(); i++)
        {
            int movieId = seeds[i];

            if (alreadyShown.count(movieId))
            {
                continue;
            }

            if (!titles.count(movieId))
            {
                continue;
            }

            uniqueSeeds.push_back(movieId);

            if ((int)uniqueSeeds.size() == 3)
            {
                break;
            }
        }

        if (uniqueSeeds.empty())
        {
            cout << "\nAll seed movies for " << getGenreDisplayName(genre)
                 << " were already shown. Skipping.\n";
            continue;
        }

        cout << "\n----------------------------------\n";
        cout << "Genre: " << getGenreDisplayName(genre) << "\n";
        cout << "Trending movies to rate:\n";

        for (int i = 0; i < (int)uniqueSeeds.size(); i++)
        {
            int movieId = uniqueSeeds[i];
            cout << "  " << (i + 1) << ". " << titles[movieId];

            if (trends.ready)
            {
                for (int j = 0; j < (int)trends.topMovies.size(); j++)
                {
                    if (trends.topMovies[j].movieId == movieId)
                    {
                        cout << "  (rated " << fixed << setprecision(2)
                             << trends.topMovies[j].bayesianAvg << "/5 by the network)";
                        break;
                    }
                }
            }

            cout << "\n";
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
            cout << "Skipped.\n";
            continue;
        }

        if (!(genreChoice == 'r' || genreChoice == 'R'))
        {
            cout << "Invalid choice. Skipping.\n";
            continue;
        }

        for (int i = 0; i < (int)uniqueSeeds.size(); i++)
        {
            int movieId = uniqueSeeds[i];
            alreadyShown.insert(movieId);

            cout << "\n"
                 << titles[movieId] << "\n";
            cout << "Rate from 1 to 5  (0 = skip): ";

            double rating;
            cin >> rating;

            while (cin.fail() || rating < 0 || rating > 5)
            {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Enter a number between 0 and 5: ";
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

static bool deleteAccount(int userId)
{
    cout << "\nAre you sure you want to delete your account? (y/n): ";

    char c1;
    cin >> c1;
    clearInputLine();

    if (c1 != 'y' && c1 != 'Y')
    {
        cout << "Deletion cancelled.\n";
        return false;
    }

    cout << "This will permanently delete your ratings and account file. Type y again to confirm: ";

    char c2;
    cin >> c2;
    clearInputLine();

    if (c2 != 'y' && c2 != 'Y')
    {
        cout << "Deletion cancelled.\n";
        return false;
    }

    string userFile = "user" + to_string(userId) + "_ratings.txt";

    ifstream fin("ratings_processed.csv");
    ofstream tmp("ratings_processed_tmp.csv");

    if (fin.is_open() && tmp.is_open())
    {
        string line;
        getline(fin, line);

        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }

        tmp << line << "\n";

        while (getline(fin, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
            {
                line.erase(line.size() - 1);
            }

            if (line.empty())
            {
                continue;
            }

            stringstream ss(line);
            string mStr, uStr, rStr;

            getline(ss, mStr, ',');
            getline(ss, uStr, ',');
            getline(ss, rStr, ',');

            bool removeRow = false;

            try
            {
                removeRow = (stoi(uStr) == userId);
            }
            catch (...)
            {
            }

            if (!removeRow)
            {
                tmp << line << "\n";
            }
        }

        fin.close();
        tmp.close();

        remove("ratings_processed.csv");
        rename("ratings_processed_tmp.csv", "ratings_processed.csv");
    }

    remove(userFile.c_str());
    markUserDeleted(userId);

    cout << "Account deleted successfully.\n";
    return true;
}

void registerUser(const TrendsData &trends)
{
    int userId = generateUserId();

    ofstream fout("user" + to_string(userId) + "_ratings.txt");
    fout.close();

    cout << "Registration successful!\n";
    cout << "Your User ID is: " << userId << "\n";
    cout << "Keep it safe!!!!!!!! you will need it to log in.\n";

    runColdStartOnboarding(userId, trends);
}

bool loginUser(const TrendsData &trends)
{
    int userId;

    cout << "Enter your User ID: ";
    cin >> userId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input.\n";
        return false;
    }

    string file = "user" + to_string(userId) + "_ratings.txt";

    ifstream fin(file);
    if (!fin)
    {
        cout << "User not found!\n";
        return false;
    }

    fin.close();

    cout << "Login successful!\n";
    userMenu(userId, trends);

    return true;
}

void userMenu(int userId, const TrendsData &trends)
{
    int choice;

    while (true)
    {
        cout << "\n=== CineConnect---- Main Menu ===\n";
        cout << "You can check your User ID in Status. Keep it safe for future logins.\n";
        cout << "1. Rate a Movie\n";
        cout << "2. My Ratings\n";
        cout << "3. Get Recommendations\n";
        cout << "4. Trending Now\n";
        cout << "5. Status\n";
        cout << "6. Delete Account\n";
        cout << "7. Logout\n";
        cout << "Enter choice: ";

        cin >> choice;

        if (cin.fail())
        {
            cout << "Invalid input! Please enter a number.\n";
            clearInputLine();
            continue;
        }

        clearInputLine();

        if (choice == 1)
        {
            rateMovie(userId);
        }
        else if (choice == 2)
        {
            showMyRatings(userId);
        }
        else if (choice == 3)
        {
            recommendMoviesForUser(userId, trends);
        }
        else if (choice == 4)
        {
            showCurrentTrends(trends);
        }
        else if (choice == 5)
        {
            cout << "Your UserID is: " << userId;
        }
        else if (choice == 6)
        {
            if (deleteAccount(userId))
            {
                break;
            }
        }
        else if (choice == 7)
        {
            break;
        }
        else
        {
            cout << "Invalid choice. Try again.\n";
        }
    }
}
