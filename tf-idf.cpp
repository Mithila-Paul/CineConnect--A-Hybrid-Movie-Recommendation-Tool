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
#include "mf.h"

using namespace std;

// for ignoring common stop words
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

        transform(word.begin(), word.end(), word.begin(), ::tolower); // transforming to lower case

        word.erase(remove_if(word.begin(), word.end(), ::ispunct), word.end()); // it is for removing punctuation

        if (!word.empty() && stopWords.find(word) == stopWords.end())
        {
            words.push_back(word); // only add if it not a stop word
        }
    }
    return words;
}

// csv reader for movie id and tags
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
    getline(file, line); // I will skip header

    while (getline(file, line))
    {
        stringstream ss(line);
        string idStr, title, tags;

        getline(ss, idStr, ','); // movie id
        getline(ss, title, ','); // title which is ignored
        getline(ss, tags);       // tags

        int movieID = stoi(idStr);

        // quotation removal
        if (!tags.empty() && tags.front() == '"')
            tags.erase(0, 1);
        if (!tags.empty() && tags.back() == '"')
            tags.pop_back();

        movies.push_back({movieID, tags});
    }

    file.close();
    return movies;
}

// tf
unordered_map<string, double> computeTF(const vector<string> &words)
{
    unordered_map<string, double> tf;

    for (const string &w : words)
        tf[w]++;

    double totalWords = words.size();
    for (auto &it : tf)
        it.second /= totalWords;

    return tf;
}

// df
unordered_map<string, int> computeDF(const vector<vector<string>> &docs)
{
    unordered_map<string, int> df;

    for (const auto &doc : docs)
    {
        unordered_set<string> uniqueWords(doc.begin(), doc.end());
        for (const string &w : uniqueWords)
            df[w]++;
    }
    return df;
}

// tf-idf
unordered_map<string, double> computeTFIDF(
    const unordered_map<string, double> &tf,
    const unordered_map<string, int> &df,
    int totalDocs)
{

    unordered_map<string, double> tfidf;

    for (auto &it : tf)
    {
        const string &word = it.first;
        double idf = log((double)totalDocs / df.at(word));
        tfidf[word] = it.second * idf;
    }

    return tfidf;
}

// save movie vectors to file
void saveMovieVectorsToFile(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    const string &filename)
{
    ofstream out(filename);

    for (auto &mv : movieVectors)
    {
        out << mv.first; // movie ID

        for (auto &it : mv.second)
        {
            out << " " << it.first << ":" << it.second;
        }
        out << "\n";
    }

    out.close();
}

// extract movie topics
unordered_map<int, vector<string>> extractMovieTopics(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    int topWords)
{
    unordered_map<int, vector<string>> movieTopics;

    for (auto &mv : movieVectors)
    {
        vector<pair<double, string>> words;

        for (auto &it : mv.second)
            words.push_back({it.second, it.first});

        sort(words.rbegin(), words.rend());

        vector<string> topics;
        for (int i = 0; i < topWords && i < words.size(); i++)
            topics.push_back(words[i].second);

        movieTopics[mv.first] = topics;
    }

    return movieTopics;
}

// save topics to file
void saveTopicsToFile(
    const unordered_map<int, vector<string>> &topics)
{
    ofstream out("movie_topics.txt");

    for (auto &mv : topics)
    {
        out << mv.first;

        for (auto &word : mv.second)
            out << " " << word;

        out << endl;
    }
}


// =============================================================================
// generateGenreFile
// Reads movies_synchronized.csv and builds movie_genres.txt.
// Format per line: movieId|primaryGenre|genre1:w1,genre2:w2,...
//
// Genre weights are computed by position of first appearance in the tags:
// Earlier-appearing genres get higher weight (inverse-rank scoring).
// Primary genre = first genre keyword found in tags.
//
// Actors appear as last 3 tokens, director as the very last token.
// These are NOT included in genre weights — they serve actor/director search.
// =============================================================================
void generateGenreFile(const string &csvFile, const string &outFile)
{
    static const unordered_set<string> GENRES = {
        "action","adventure","animation","comedy","crime","documentary",
        "drama","fantasy","history","horror","music","mystery","romance",
        "sciencefiction","thriller","war","western","family","sport"
    };

    ifstream file(csvFile);
    if (!file.is_open()) { cout << "Genre gen: cannot open " << csvFile << "\n"; return; }

    ofstream out(outFile);
    string line;
    getline(file, line); // skip header

    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        stringstream ss(line);
        string idStr, title, tags;
        getline(ss, idStr,  ',');
        getline(ss, title,  ',');
        getline(ss, tags);
        tags.erase(remove(tags.begin(), tags.end(), '"'), tags.end());

        string tagsLower = tags;
        transform(tagsLower.begin(), tagsLower.end(), tagsLower.begin(), ::tolower);

        // Find genre keywords as exact space-separated tokens, preserve order
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

        // Default to drama if no genre found
        if (found.empty()) found.push_back("drama");

        // Inverse-rank weights: first genre gets n points, second n-1, etc.
        int n = found.size();
        double total = n * (n + 1) / 2.0;

        string primary = found[0];
        string weightStr;
        // Sort by weight descending for output
        vector<pair<double,string>> weighted;
        for (int i = 0; i < n; i++)
            weighted.push_back({(n - i) / total, found[i]});
        sort(weighted.rbegin(), weighted.rend());

        for (int i = 0; i < (int)weighted.size(); i++)
        {
            if (i > 0) weightStr += ",";
            // format weight to 4 decimal places
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

void initializeSystem()
{
    // Skip recompute if vectors already exist — saves several seconds on every launch
    {

        
cout << "Checking system files...\n";
        ifstream check("movie_vectors.txt");
        if (check.is_open())
        {
            check.close();
            // Also generate genre file if missing (lightweight, always safe to regenerate)
            ifstream gcheck("movie_genres.txt");
            if (!gcheck.is_open())
                generateGenreFile("movies_synchronized.csv", "movie_genres.txt");
            // Train MF once if model file missing, otherwise loads instantly
            initMF("ratings_processed.csv");
            cout << "System data already initialized. Ready!\n";
            return;
        }
    }

    string filename = "movies_synchronized.csv";
    cout << "Initializing system data... this may take a moment.\n";

    // read movie id + tags
    vector<pair<int, string>> movies = readMoviesFromCSV(filename);
    int N = movies.size();

    // tokenization
    vector<vector<string>> tokenizedDocs;
    for (auto &m : movies)
        tokenizedDocs.push_back(tokenize(m.second));

    // df
    unordered_map<string, int> df = computeDF(tokenizedDocs);

    // tf-idf with real movie id
    vector<pair<int, unordered_map<string, double>>> movieVectors;

    for (int i = 0; i < N; i++)
    {
        auto tf = computeTF(tokenizedDocs[i]);
        auto tfidf = computeTFIDF(tf, df, N);
        movieVectors.push_back({movies[i].first, tfidf});
    }

    // save
    saveMovieVectorsToFile(movieVectors, "movie_vectors.txt");
    auto topics = extractMovieTopics(movieVectors);
    saveTopicsToFile(topics);
    generateGenreFile("movies_synchronized.csv", "movie_genres.txt");
    // Train MF model on first run — saved to mf_model.bin for instant reuse
    initMF("ratings_processed.csv");

    cout << "System initialized! Data files created successfully.\n";
}
