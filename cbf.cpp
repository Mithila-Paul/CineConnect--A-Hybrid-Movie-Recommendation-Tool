#include "cbf.h"
#include "tf-idf.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace std;

// to load the movie vectors from file
vector<pair<int, Vector>> loadMovieVectors(const string &filename)
{
    ifstream file(filename);
    vector<pair<int, Vector>> movieVectors;
    if (!file.is_open())
    {
        cout << "Error opening movie vectors file\n";
        return movieVectors;
    }

    string line;
    while (getline(file, line))
    {
        stringstream ss(line);
        int movieID;
        ss >> movieID;

        Vector vec;
        string token;
        while (ss >> token)
        {
            int pos = token.find(':');
            string word = token.substr(0, pos);
            double value = stod(token.substr(pos + 1));
            vec[word] = value;
        }

        movieVectors.push_back({movieID, vec});
    }

    file.close();
    return movieVectors;
}

Vector buildQueryVector(
    const string &query,
    const unordered_map<string, int> &df,
    int totalDocs)
{
    vector<string> tokens = tokenize(query);

    auto tf = computeTF(tokens);

    return computeTFIDF(tf, df, totalDocs);
}

// to add cold start recommendation, currently minimal
vector<int> coldStartRecommend(
    const vector<pair<int, Vector>> &movieVectors,
    const Vector &queryVector, // <--- Add this argument
    int topN)
{
    vector<pair<double, int>> scores;

    for (auto &mv : movieVectors)
    {
        // Calculate similarity purely based on the text search
        double score = cosineSimilarity(queryVector, mv.second);
        scores.push_back({score, mv.first});
    }

    sort(scores.rbegin(), scores.rend());

    vector<int> recs;
    for (int i = 0; i < topN && i < scores.size(); i++)
        recs.push_back(scores[i].second);

    return recs;
}
// detect cold start
bool isColdStart(const Vector &userVector)
{
    return userVector.empty();
}

// to get already rated movie IDs so that I can exclude them from recommendations
unordered_set<int>
getRatedMovieIds(const string &ratingsFile)
{
    unordered_set<int> rated;
    ifstream in(ratingsFile);

    int movieId;
    double rating;
    string title, bar;

    while (in >> movieId >> bar)
    {
        getline(in, title, '|'); // ignore title
        in >> rating;
        rated.insert(movieId);
    }
    in.close();

    return rated;
}

// building user preference vector
Vector buildUserVector(
    const string &ratingsFile,
    const vector<pair<int, Vector>> &movieVectors)
{
    ifstream file(ratingsFile);
    Vector userVector;

    if (!file.is_open())
    {
        cout << "Error opening user ratings file\n";
        return userVector;
    }

    int movieID;
    double rating;
    string title, bar;

    while (file >> movieID >> bar)
    {
        getline(file, title, '|'); // skip title
        file >> rating;

        for (auto &mv : movieVectors)
        {
            if (mv.first == movieID)
            {
                for (auto &it : mv.second)
                {
                    userVector[it.first] += rating * it.second;
                }
                break;
            }
        }
    }

    // Normalizing user vector
    double norm = 0.0;
    for (auto &it : userVector)
        norm += it.second * it.second;

    norm = sqrt(norm);
    if (norm > 0)
    {
        for (auto &it : userVector)
            it.second /= norm;
    }

    return userVector;
}

// load movie titles from CSV to map them with ids
unordered_map<int, string>
loadMovieTitles(const string &csvFile)
{
    unordered_map<int, string> titles;
    ifstream file(csvFile);

    string line;
    getline(file, line); // header

    while (getline(file, line))
    {
        stringstream ss(line);
        string idStr, title;

        getline(ss, idStr, ',');
        getline(ss, title, ',');

        int id = stoi(idStr);

        // remove quotes
        if (!title.empty() && title.front() == '"')
            title = title.substr(1, title.size() - 2);

        titles[id] = title;
    }
    return titles;
}

// calculating cosine similarity
double cosineSimilarity(const Vector &A, const Vector &B)
{
    double dot = 0.0, normA = 0.0, normB = 0.0;

    for (auto &it : A)
    {
        normA += it.second * it.second;
        if (B.count(it.first))
            dot += it.second * B.at(it.first);
    }

    for (auto &it : B)
        normB += it.second * it.second;

    if (normA == 0 || normB == 0)
        return 0.0;

    return dot / (sqrt(normA) * sqrt(normB));
}

// cbf recommendation function on basis of users personal ratings, suggesting top 10 movies
vector<int> recommendMovies(
    const Vector &userVector,
    const Vector &queryVector,
    const vector<pair<int, Vector>> &movieVectors,
    int topN,
    const unordered_set<int> &ratedMovies,
    const vector<string> &queryTopics)
{
    auto movieTopics = loadMovieTopics("movie_topics.txt");

    // vector<string> queryTopics = processQuery(""); // placeholder

    vector<pair<double, int>> scores;

    double alpha = 0.5; // user preference weight
    double beta = 0.3;  // search query weight
    double gamma = 0.2; // topic similarity weight

    for (auto &mv : movieVectors)
    {
        int movieId = mv.first;

        if (ratedMovies.count(movieId))
            continue;

        double userSim = cosineSimilarity(userVector, mv.second);
        double querySim = cosineSimilarity(queryVector, mv.second);

        double topicSim = 0.0;
        if (movieTopics.count(movieId))
            topicSim = topicSimilarity(queryTopics, movieTopics[movieId]);

        double finalScore =
            alpha * userSim +
            beta * querySim +
            gamma * topicSim;

        scores.push_back({finalScore, movieId});
    }

    sort(scores.rbegin(), scores.rend());

    vector<int> recs;

    for (int i = 0; i < topN && i < scores.size(); i++)
        recs.push_back(scores[i].second);

    return recs;
}

unordered_map<int, vector<string>>
loadMovieTopics(const string &file)
{
    unordered_map<int, vector<string>> topics;
    ifstream in(file);

    string line;
    while (getline(in, line))
    {
        stringstream ss(line);
        int id;
        ss >> id;

        string word;
        while (ss >> word)
            topics[id].push_back(word);
    }

    return topics;
}
vector<string> processQuery(string query)
{
    return tokenize(query);
}

// Jaccard Similarity for topics (intersection over union)
double topicSimilarity(const vector<string> &A, const vector<string> &B)
{
    if (A.empty() || B.empty())
        return 0.0;

    unordered_set<string> setA(A.begin(), A.end());
    unordered_set<string> setB(B.begin(), B.end());

    int intersection = 0;
    for (const string &s : setB)
    {
        if (setA.count(s))
            intersection++;
    }

    int unionSize = setA.size() + setB.size() - intersection;

    if (unionSize == 0)
        return 0.0;

    return (double)intersection / unionSize;
}
