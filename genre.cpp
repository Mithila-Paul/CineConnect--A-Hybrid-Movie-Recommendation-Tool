#include "genre.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

using namespace std;

// =============================================================================
// normaliseName
// Converts a human-readable name to the concatenated lowercase format used
// in tags. "Leonardo DiCaprio" -> "leonardodicaprio"
// "Christopher Nolan"         -> "christophernolan"
// Also handles already-normalised input gracefully.
// =============================================================================
string normaliseName(const string &name)
{
    string result;
    for (char c : name)
    {
        if (c == ' ' || c == '-') continue; // strip spaces and hyphens
        result += tolower(c);
    }
    return result;
}

// =============================================================================
// loadGenreMap
// Reads movie_genres.txt into memory.
// Format: movieId|primaryGenre|genre1:w1,genre2:w2,...
// =============================================================================
GenreMap loadGenreMap(const string &filename)
{
    GenreMap gmap;
    ifstream file(filename);
    if (!file.is_open())
    {
        cout << "Genre: cannot open " << filename << "\n";
        return gmap;
    }

    string line;
    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        stringstream ss(line);
        string midStr, primary, weightStr;
        getline(ss, midStr,    '|');
        getline(ss, primary,   '|');
        getline(ss, weightStr, '|');

        if (midStr.empty()) continue;

        MovieGenreInfo info;
        try { info.movieId = stoi(midStr); } catch (...) { continue; }
        info.primaryGenre = primary;

        // Parse "genre1:w1,genre2:w2,..."
        stringstream ws(weightStr);
        string pair;
        while (getline(ws, pair, ','))
        {
            auto colon = pair.find(':');
            if (colon == string::npos) continue;
            string genre  = pair.substr(0, colon);
            string wStr   = pair.substr(colon + 1);
            try { info.weights[genre] = stod(wStr); } catch (...) {}
        }

        gmap[info.movieId] = info;
    }

    file.close();
    return gmap;
}

// =============================================================================
// getMoviesByPrimaryGenre
// Returns all movieIds where primaryGenre matches the requested genre.
// This is the "strict" filter — only primary-genre movies.
// Used as the first candidate set in genre search, then expanded if needed.
// =============================================================================
vector<int> getMoviesByPrimaryGenre(const string &genre, const GenreMap &gmap)
{
    string g = genre;
    transform(g.begin(), g.end(), g.begin(), ::tolower);
    // handle "science fiction" -> "sciencefiction"
    g.erase(remove(g.begin(), g.end(), ' '), g.end());

    vector<int> result;
    for (auto &kv : gmap)
        if (kv.second.primaryGenre == g)
            result.push_back(kv.first);

    return result;
}

// =============================================================================
// getMoviesByGenreThreshold
// Returns all movieIds where the genre weight >= threshold.
// Broader than primary-only — captures cross-genre movies.
// Default threshold 0.15 means "this genre contributes at least 15% of identity".
// =============================================================================
vector<int> getMoviesByGenreThreshold(
    const string &genre, const GenreMap &gmap, double threshold)
{
    string g = genre;
    transform(g.begin(), g.end(), g.begin(), ::tolower);
    g.erase(remove(g.begin(), g.end(), ' '), g.end());

    vector<int> result;
    for (auto &kv : gmap)
    {
        auto it = kv.second.weights.find(g);
        if (it != kv.second.weights.end() && it->second >= threshold)
            result.push_back(kv.first);
    }
    return result;
}

// =============================================================================
// getMoviesByActor
// Searches the CSV tags for the normalised actor name.
// Actors appear as space-separated concatenated tokens near the end of tags.
// Returns all movieIds that contain the normalised name as an exact token.
// =============================================================================
vector<int> getMoviesByActor(const string &actorName, const string &csvFile)
{
    string target = normaliseName(actorName);
    vector<int> result;

    ifstream file(csvFile);
    if (!file.is_open()) return result;

    string line;
    getline(file, line); // skip header

    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        stringstream ss(line);
        string idStr, title, tags;
        getline(ss, idStr,  ',');
        getline(ss, title,  ',');
        getline(ss, tags);

        tags.erase(remove(tags.begin(), tags.end(), '"'), tags.end());
        string tagsLower = tags;
        transform(tagsLower.begin(), tagsLower.end(), tagsLower.begin(), ::tolower);

        // Search for exact token match (space-delimited)
        stringstream ts(tagsLower);
        string tok;
        while (ts >> tok)
        {
            // strip punctuation from token
            tok.erase(remove_if(tok.begin(), tok.end(), ::ispunct), tok.end());
            if (tok == target)
            {
                try { result.push_back(stoi(idStr)); } catch (...) {}
                break;
            }
        }
    }
    file.close();
    return result;
}

// =============================================================================
// getMoviesByDirector
// Director is ALWAYS the last space-separated token in the tags field.
// This is consistent across all 4809 movies in the dataset.
// =============================================================================
vector<int> getMoviesByDirector(const string &directorName, const string &csvFile)
{
    string target = normaliseName(directorName);
    vector<int> result;

    ifstream file(csvFile);
    if (!file.is_open()) return result;

    string line;
    getline(file, line); // skip header

    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        stringstream ss(line);
        string idStr, title, tags;
        getline(ss, idStr,  ',');
        getline(ss, title,  ',');
        getline(ss, tags);

        tags.erase(remove(tags.begin(), tags.end(), '"'), tags.end());
        string tagsLower = tags;
        transform(tagsLower.begin(), tagsLower.end(), tagsLower.begin(), ::tolower);

        // Get the last token — that is the director
        stringstream ts(tagsLower);
        string tok, lastTok;
        while (ts >> tok)
        {
            tok.erase(remove_if(tok.begin(), tok.end(), ::ispunct), tok.end());
            if (!tok.empty()) lastTok = tok;
        }

        if (lastTok == target)
        {
            try { result.push_back(stoi(idStr)); } catch (...) {}
        }
    }
    file.close();
    return result;
}

// =============================================================================
// getMoviesByPlot
// Splits the query into keywords and returns movies whose tags contain ALL of them.
// Matches against the full tags string (plot + keywords), case-insensitive.
// =============================================================================
vector<int> getMoviesByPlot(const string &plotQuery, const string &csvFile)
{
    // Split query into keywords
    vector<string> keywords;
    stringstream qs(plotQuery);
    string kw;
    while (qs >> kw)
    {
        transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
        kw.erase(remove_if(kw.begin(), kw.end(), ::ispunct), kw.end());
        if (kw.size() > 2) // ignore very short words
            keywords.push_back(kw);
    }
    if (keywords.empty()) return {};

    vector<int> result;
    ifstream file(csvFile);
    if (!file.is_open()) return result;

    string line;
    getline(file, line); // skip header

    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        stringstream ss(line);
        string idStr, title, tags;
        getline(ss, idStr,  ',');
        getline(ss, title,  ',');
        getline(ss, tags);

        tags.erase(remove(tags.begin(), tags.end(), '"'), tags.end());
        string tagsLower = tags;
        transform(tagsLower.begin(), tagsLower.end(), tagsLower.begin(), ::tolower);

        // Check all keywords are present
        bool allFound = true;
        for (auto &k : keywords)
        {
            if (tagsLower.find(k) == string::npos)
            { allFound = false; break; }
        }

        if (allFound)
            try { result.push_back(stoi(idStr)); } catch (...) {}
    }
    file.close();
    return result;
}

// =============================================================================
// getGenreWeight
// O(1) lookup: what weight does this genre have for this movie?
// Returns 0.0 if the movie has no entry or the genre is not present.
// =============================================================================
double getGenreWeight(int movieId, const string &genre, const GenreMap &gmap)
{
    auto it = gmap.find(movieId);
    if (it == gmap.end()) return 0.0;
    auto jt = it->second.weights.find(genre);
    if (jt == it->second.weights.end()) return 0.0;
    return jt->second;
}