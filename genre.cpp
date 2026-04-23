#include "genre.h"
#include "csv_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

using namespace std;

const std::vector<std::string> ALL_GENRES = { // egula hard code korai lagbe :)))
    "action", "adventure", "animation", "comedy", "crime",
    "documentary", "drama", "fantasy", "history", "horror",
    "music", "mystery", "romance", "sciencefiction", "thriller",
    "war", "western", "family", "sport"
};

const std::vector<std::pair<std::string, std::string>> GENRE_DISPLAY = {
    {"action", "Action"},
    {"adventure", "Adventure"},
    {"animation", "Animation"},
    {"comedy", "Comedy"},
    {"crime", "Crime"},
    {"documentary", "Documentary"},
    {"drama", "Drama"},
    {"fantasy", "Fantasy"},
    {"history", "History"},
    {"horror", "Horror"},
    {"music", "Music"},
    {"mystery", "Mystery"},
    {"romance", "Romance"},
    {"sciencefiction", "Science Fiction"},
    {"thriller", "Thriller"},
    {"war", "War"},
    {"western", "Western"},
    {"family", "Family"},
    {"sport", "Sport"}
};

string normaliseName(const string &name)
{
    string result;

    for (int i = 0; i < (int)name.size(); i++)
    {
        char c = name[i];

        if (c == ' ' || c == '-')
        {
            continue;
        }

        result += (char)tolower((unsigned char)c);
    }

    return result;
}

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
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }

        if (line.empty())
        {
            continue;
        }

        stringstream ss(line);
        string midStr, primary, weightStr;

        getline(ss, midStr, '|');
        getline(ss, primary, '|');
        getline(ss, weightStr, '|');

        if (midStr.empty())
        {
            continue;
        }

        MovieGenreInfo info;

        try
        {
            info.movieId = stoi(midStr);
        }
        catch (...)
        {
            continue;
        }

        info.primaryGenre = primary;

        stringstream ws(weightStr);
        string pair;

        while (getline(ws, pair, ','))
        {
            size_t colon = pair.find(':');
            if (colon == string::npos)
            {
                continue;
            }

            string genre = pair.substr(0, colon);
            string wStr = pair.substr(colon + 1);

            try
            {
                info.weights[genre] = stod(wStr);
            }
            catch (...)
            {
            }
        }

        gmap[info.movieId] = info;
    }

    file.close();
    return gmap;
}

vector<int> getMoviesByPrimaryGenre(const string &genre, const GenreMap &gmap)
{
    string g = genre;
    transform(g.begin(), g.end(), g.begin(), ::tolower);
    g.erase(remove(g.begin(), g.end(), ' '), g.end());

    vector<int> result;

    GenreMap::const_iterator it;
    for (it = gmap.begin(); it != gmap.end(); ++it)
    {
        if (it->second.primaryGenre == g)
        {
            result.push_back(it->first);
        }
    }

    return result;
}

vector<int> getMoviesByGenreThreshold(
    const string &genre, const GenreMap &gmap, double threshold)
{
    string g = genre;
    transform(g.begin(), g.end(), g.begin(), ::tolower);
    g.erase(remove(g.begin(), g.end(), ' '), g.end());

    vector<int> result;

    GenreMap::const_iterator it;
    for (it = gmap.begin(); it != gmap.end(); ++it)
    {
        unordered_map<string, double>::const_iterator wt = it->second.weights.find(g);
        if (wt != it->second.weights.end() && wt->second >= threshold)
        {
            result.push_back(it->first);
        }
    }

    return result;
}

vector<int> getMoviesByActor(const string &actorName, const string &csvFile)
{
    string target = normaliseName(actorName);
    vector<int> result;

    ifstream file(csvFile);
    if (!file.is_open())
    {
        return result;
    }

    string line;
    getline(file, line); // skip header

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
        tags.erase(remove(tags.begin(), tags.end(), '"'), tags.end());

        string tagsLower = tags;
        transform(tagsLower.begin(), tagsLower.end(), tagsLower.begin(), ::tolower);

        stringstream ts(tagsLower);
        string tok;

        while (ts >> tok)
        {
            tok.erase(remove_if(tok.begin(), tok.end(), ::ispunct), tok.end());

            if (tok == target)
            {
                try
                {
                    result.push_back(stoi(idStr));
                }
                catch (...)
                {
                }
                break;
            }
        }
    }

    file.close();
    return result;
}

vector<int> getMoviesByDirector(const string &directorName, const string &csvFile)
{
    string target = normaliseName(directorName);
    vector<int> result;

    ifstream file(csvFile);
    if (!file.is_open())
    {
        return result;
    }

    string line;
    getline(file, line); // skip header

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
        tags.erase(remove(tags.begin(), tags.end(), '"'), tags.end());

        string tagsLower = tags;
        transform(tagsLower.begin(), tagsLower.end(), tagsLower.begin(), ::tolower);

        stringstream ts(tagsLower);
        string tok;
        string lastTok;

        while (ts >> tok)
        {
            tok.erase(remove_if(tok.begin(), tok.end(), ::ispunct), tok.end());

            if (!tok.empty())
            {
                lastTok = tok;
            }
        }

        if (lastTok == target)
        {
            try
            {
                result.push_back(stoi(idStr));
            }
            catch (...)
            {
            }
        }
    }

    file.close();
    return result;
}

vector<int> getMoviesByPlot(const string &plotQuery, const string &csvFile)
{
    vector<string> keywords;
    stringstream qs(plotQuery);
    string kw;

    while (qs >> kw)
    {
        transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
        kw.erase(remove_if(kw.begin(), kw.end(), ::ispunct), kw.end());

        if (kw.size() > 2)
        {
            keywords.push_back(kw);
        }
    }

    if (keywords.empty())
    {
        return vector<int>();
    }

    int totalKeywords = (int)keywords.size();

    vector<pair<int, int>> scored;

    ifstream file(csvFile);
    if (!file.is_open())
    {
        return vector<int>();
    }

    string line;
    getline(file, line); // skip header

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
        tags.erase(remove(tags.begin(), tags.end(), '"'), tags.end());

        string tagsLower = tags;
        transform(tagsLower.begin(), tagsLower.end(), tagsLower.begin(), ::tolower);

        int hits = 0;

        for (int i = 0; i < totalKeywords; i++)
        {
            if (tagsLower.find(keywords[i]) != string::npos)
            {
                hits++;
            }
        }

        if (hits == 0)
        {
            continue;
        }

        int movieId = -1;
        try
        {
            movieId = stoi(idStr);
        }
        catch (...)
        {
            continue;
        }

        scored.push_back(make_pair(hits, movieId));
    }

    file.close();

    stable_sort(
        scored.begin(),
        scored.end(),
        [](const pair<int, int> &a, const pair<int, int> &b)
        {
            return a.first > b.first;
        });

    vector<int> result;
    result.reserve(scored.size());

    for (int i = 0; i < (int)scored.size(); i++)
    {
        result.push_back(scored[i].second);
    }

    return result;
}

double getGenreWeight(int movieId, const string &genre, const GenreMap &gmap)
{
    GenreMap::const_iterator it = gmap.find(movieId);
    if (it == gmap.end())
    {
        return 0.0;
    }

    unordered_map<string, double>::const_iterator jt = it->second.weights.find(genre);
    if (jt == it->second.weights.end())
    {
        return 0.0;
    }

    return jt->second;
}
