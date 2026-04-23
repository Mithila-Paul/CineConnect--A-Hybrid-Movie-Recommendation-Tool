#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include "tf-idf.h"
#include "csv_parser.h"

using namespace std;

// Common words to ignore
unordered_set<string> stopWords = {
    "the", "is", "a", "an", "and", "or", "of", "to", "in", "on", "for", "with",
    "by", "at", "from", "as", "that", "this", "it", "was", "are", "be",
    "but", "between"};

vector<string> tokenize(const string &text)
{
    stringstream ss(text);
    vector<string> words;
    string word;

    while (ss >> word)
    {
        transform(word.begin(), word.end(), word.begin(), ::tolower);

        word.erase(
            remove_if(word.begin(), word.end(), ::ispunct),
            word.end());

        if (!word.empty() && stopWords.find(word) == stopWords.end())
        {
            words.push_back(word);
        }
    }

    return words;
}

vector<pair<int, string>> readMoviesFromCSV(const string &filename)
{
    ifstream file(filename);
    vector<pair<int, string>> movies;

    if (!file.is_open())
    {
        cout << "Error opening file\n";
        return movies;
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
            tags.erase(0, 1);
        }

        if (!tags.empty() && tags[tags.size() - 1] == '"')
        {
            tags.erase(tags.size() - 1);
        }

        try
        {
            movies.push_back(make_pair(stoi(idStr), tags));
        }
        catch (...)
        {
        }
    }

    file.close();
    return movies;
}

unordered_map<string, double> computeTF(const vector<string> &words)
{
    unordered_map<string, double> tf;

    for (int i = 0; i < (int)words.size(); i++)
    {
        tf[words[i]]++;
    }

    double totalWords = (double)words.size();

    unordered_map<string, double>::iterator it;
    for (it = tf.begin(); it != tf.end(); ++it)
    {
        it->second /= totalWords;
    }

    return tf;
}

unordered_map<string, int> computeDF(const vector<vector<string>> &docs)
{
    unordered_map<string, int> df;

    for (int i = 0; i < (int)docs.size(); i++)
    {
        unordered_set<string> uniqueWords(docs[i].begin(), docs[i].end());

        unordered_set<string>::iterator it;
        for (it = uniqueWords.begin(); it != uniqueWords.end(); ++it)
        {
            df[*it]++;
        }
    }

    return df;
}

unordered_map<string, double> computeTFIDF(
    const unordered_map<string, double> &tf,
    const unordered_map<string, int> &df,
    int totalDocs)
{
    unordered_map<string, double> tfidf;

    unordered_map<string, double>::const_iterator it;
    for (it = tf.begin(); it != tf.end(); ++it)
    {
        const string &word = it->first;

        unordered_map<string, int>::const_iterator dfIt = df.find(word);
        if (dfIt == df.end() || dfIt->second == 0)
        {
            continue;
        }

        double idf = log((double)totalDocs / dfIt->second);
        tfidf[word] = it->second * idf;
    }

    return tfidf;
}

void saveMovieVectorsToFile(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    const string &filename)
{
    ofstream out(filename);

    for (int i = 0; i < (int)movieVectors.size(); i++)
    {
        out << movieVectors[i].first;

        unordered_map<string, double>::const_iterator it;
        for (it = movieVectors[i].second.begin(); it != movieVectors[i].second.end(); ++it)
        {
            out << " " << it->first << ":" << it->second;
        }

        out << "\n";
    }

    out.close();
}

unordered_map<int, vector<string>> extractMovieTopics(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    int topWords)
{
    unordered_map<int, vector<string>> movieTopics;

    for (int i = 0; i < (int)movieVectors.size(); i++)
    {
        vector<pair<double, string>> words;

        unordered_map<string, double>::const_iterator it;
        for (it = movieVectors[i].second.begin(); it != movieVectors[i].second.end(); ++it)
        {
            words.push_back(make_pair(it->second, it->first));
        }

        sort(words.rbegin(), words.rend());

        vector<string> topics;
        for (int j = 0; j < topWords && j < (int)words.size(); j++)
        {
            topics.push_back(words[j].second);
        }

        movieTopics[movieVectors[i].first] = topics;
    }

    return movieTopics;
}

void saveTopicsToFile(
    const unordered_map<int, vector<string>> &topics)
{
    ofstream out("movie_topics.txt");

    unordered_map<int, vector<string>>::const_iterator it;
    for (it = topics.begin(); it != topics.end(); ++it)
    {
        out << it->first;

        for (int i = 0; i < (int)it->second.size(); i++)
        {
            out << " " << it->second[i];
        }

        out << endl;
    }

    out.close();
}

void generateGenreFile(const string &csvFile, const string &outFile)
{
    static const unordered_set<string> GENRES = {
        "action", "adventure", "animation", "comedy", "crime", "documentary",
        "drama", "fantasy", "history", "horror", "music", "mystery", "romance",
        "sciencefiction", "thriller", "war", "western", "family", "sport"};

    ifstream file(csvFile);
    if (!file.is_open())
    {
        cout << "Genre gen: cannot open " << csvFile << "\n";
        return;
    }

    ofstream out(outFile);

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

        vector<string> found;
        unordered_set<string> seen;

        stringstream ts(tagsLower);
        string tok;

        while (ts >> tok)
        {
            tok.erase(remove_if(tok.begin(), tok.end(), ::ispunct), tok.end());

            if (GENRES.count(tok) && !seen.count(tok))
            {
                found.push_back(tok);
                seen.insert(tok);
            }
        }

        if (found.empty())
        {
            found.push_back("drama");
        }

        int n = (int)found.size();
        double total = n * (n + 1) / 2.0;

        string primary = found[0];
        string weightStr;

        vector<pair<double, string>> weighted;
        for (int i = 0; i < n; i++)
        {
            weighted.push_back(make_pair((n - i) / total, found[i]));
        }

        sort(weighted.rbegin(), weighted.rend());

        for (int i = 0; i < (int)weighted.size(); i++)
        {
            if (i > 0)
            {
                weightStr += ",";
            }

            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f", weighted[i].first);
            weightStr += weighted[i].second + ":" + buf;
        }

        out << idStr << "|" << primary << "|" << weightStr << "\n";
    }

    file.close();
    out.close();

    cout << "Genre file generated: " << outFile << "\n";
}

static unordered_map<string, int> g_df;
static int g_totalDocs = 0;

const unordered_map<string, int> &getGlobalDF()
{
    return g_df;
}

int getGlobalTotalDocs()
{
    return g_totalDocs;
}

void initializeTFIDFCache()
{
    if (!g_df.empty() && g_totalDocs > 0)
    {
        return;
    }

    vector<pair<int, string>> movies = readMoviesFromCSV("movies_synchronized.csv");

    vector<vector<string>> tokenizedDocs;
    tokenizedDocs.reserve(movies.size());

    for (int i = 0; i < (int)movies.size(); i++)
    {
        tokenizedDocs.push_back(tokenize(movies[i].second));
    }

    g_df = computeDF(tokenizedDocs);
    g_totalDocs = (int)movies.size();
}
