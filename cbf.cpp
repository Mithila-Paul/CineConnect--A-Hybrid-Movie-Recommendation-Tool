#include "cbf.h"
#include "tf-idf.h"
#include "csv_parser.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace std;

// Load saved movie vectors from file
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
            int pos = (int)token.find(':');
            string word = token.substr(0, pos);
            double value = stod(token.substr(pos + 1));
            vec[word] = value;
        }

        movieVectors.push_back(make_pair(movieID, vec));
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
    unordered_map<string, double> tf = computeTF(tokens);
    return computeTFIDF(tf, df, totalDocs);
}

// Cold-start recommendation using only query similarity
vector<int> coldStartRecommend(
    const vector<pair<int, Vector>> &movieVectors,
    const Vector &queryVector,
    int topN)
{
    vector<pair<double, int>> scores;

    for (int i = 0; i < (int)movieVectors.size(); i++)
    {
        double score = cosineSimilarity(queryVector, movieVectors[i].second);
        scores.push_back(make_pair(score, movieVectors[i].first));
    }

    sort(scores.rbegin(), scores.rend());

    vector<int> recs;
    for (int i = 0; i < topN && i < (int)scores.size(); i++)
    {
        recs.push_back(scores[i].second);
    }

    return recs;
}

bool isColdStart(const Vector &userVector)
{
    return userVector.empty();
}

// Get all movie ids already rated by this user
unordered_set<int> getRatedMovieIds(const string &ratingsFile, int userId)
{
    unordered_set<int> rated;

    ifstream in(ratingsFile);
    if (in.is_open())
    {
        string line;

        while (getline(in, line))
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
                    rated.insert(stoi(midStr));
                }
                catch (...)
                {
                }
            }
        }

        in.close();
    }

    // Also check global ratings file for original dataset users
    if (userId >= 1 && userId <= 610)
    {
        ifstream global("ratings_processed.csv");

        if (global.is_open())
        {
            string line;
            getline(global, line); // skip header

            while (getline(global, line))
            {
                if (!line.empty() && line[line.size() - 1] == '\r')
                {
                    line.erase(line.size() - 1);
                }

                stringstream ss(line);
                string movieIdStr, userIdStr;

                getline(ss, movieIdStr, ',');
                getline(ss, userIdStr, ',');

                if (!userIdStr.empty() && !movieIdStr.empty())
                {
                    try
                    {
                        if (stoi(userIdStr) == userId)
                        {
                            rated.insert(stoi(movieIdStr));
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }

            global.close();
        }
    }

    return rated;
}

// Build user profile vector from rated movies
Vector buildUserVector(
    const string &ratingsFile,
    const vector<pair<int, Vector>> &movieVectors,
    int userId)
{
    Vector userVector;

    unordered_map<int, const Vector *> vectorMap;
    for (int i = 0; i < (int)movieVectors.size(); i++)
    {
        vectorMap[movieVectors[i].first] = &movieVectors[i].second;
    }

    if (userId >= 1 && userId <= 610)
    {
        ifstream file("ratings_processed.csv");

        if (!file.is_open())
        {
            cout << "Error opening global ratings file\n";
            return userVector;
        }

        string line;
        getline(file, line); // skip header

        while (getline(file, line))
        {
            stringstream ss(line);
            string movieIdStr, userIdStr, ratingStr;

            getline(ss, movieIdStr, ',');
            getline(ss, userIdStr, ',');
            getline(ss, ratingStr, ',');

            if (!userIdStr.empty() && stoi(userIdStr) == userId)
            {
                try
                {
                    int movieID = stoi(movieIdStr);
                    double rating = stod(ratingStr);

                    unordered_map<int, const Vector *>::iterator it = vectorMap.find(movieID);
                    if (it != vectorMap.end())
                    {
                        unordered_map<string, double>::const_iterator kv;
                        for (kv = it->second->begin(); kv != it->second->end(); ++kv)
                        {
                            userVector[kv->first] += rating * kv->second;
                        }
                    }
                }
                catch (...)
                {
                }
            }
        }

        file.close();
    }
    else
    {
        ifstream file(ratingsFile);

        if (!file.is_open())
        {
            cout << "Error opening user ratings file\n";
            return userVector;
        }

        string line;
        while (getline(file, line))
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
            rStr.erase(remove_if(rStr.begin(), rStr.end(), ::isspace), rStr.end());

            if (!midStr.empty() && !rStr.empty())
            {
                try
                {
                    int movieID = stoi(midStr);
                    double rating = stod(rStr);

                    unordered_map<int, const Vector *>::iterator it = vectorMap.find(movieID);
                    if (it != vectorMap.end())
                    {
                        unordered_map<string, double>::const_iterator kv;
                        for (kv = it->second->begin(); kv != it->second->end(); ++kv)
                        {
                            userVector[kv->first] += rating * kv->second;
                        }
                    }
                }
                catch (...)
                {
                }
            }
        }

        file.close();
    }

    // Normalize the user vector
    double norm = 0.0;

    unordered_map<string, double>::iterator it;
    for (it = userVector.begin(); it != userVector.end(); ++it)
    {
        norm += it->second * it->second;
    }

    norm = sqrt(norm);

    if (norm > 0.0)
    {
        for (it = userVector.begin(); it != userVector.end(); ++it)
        {
            it->second /= norm;
        }
    }

    return userVector;
}

// Load movie titles from csv
unordered_map<int, string> loadMovieTitles(const string &csvFile)
{
    unordered_map<int, string> titles;

    ifstream file(csvFile);
    if (!file.is_open())
    {
        return titles;
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

        try
        {
            titles[stoi(idStr)] = title;
        }
        catch (...)
        {
        }
    }

    file.close();
    return titles;
}

// Cosine similarity between two vectors
double cosineSimilarity(const Vector &A, const Vector &B)
{
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;

    unordered_map<string, double>::const_iterator it;
    for (it = A.begin(); it != A.end(); ++it)
    {
        normA += it->second * it->second;

        if (B.count(it->first))
        {
            dot += it->second * B.at(it->first);
        }
    }

    for (it = B.begin(); it != B.end(); ++it)
    {
        normB += it->second * it->second;
    }

    if (normA == 0.0 || normB == 0.0)
    {
        return 0.0;
    }

    return dot / (sqrt(normA) * sqrt(normB));
}

// CBF recommendation function
// vector<int> recommendMovies(
//     const Vector &userVector,
//     const Vector &queryVector,
//     const vector<pair<int, Vector>> &movieVectors,
//     int topN,
//     const unordered_set<int> &ratedMovies,
//     const vector<string> &queryTopics)
// {
//     unordered_map<int, vector<string>> movieTopics = loadMovieTopics("movie_topics.txt");

//     vector<pair<double, int>> scores;

//     double alpha = 0.55;
//     double beta = 0.35;
//     double gamma = 0.10;

//     for (int i = 0; i < (int)movieVectors.size(); i++)
//     {
//         int movieId = movieVectors[i].first;

//         if (ratedMovies.count(movieId))
//         {
//             continue;
//         }

//         double userSim = cosineSimilarity(userVector, movieVectors[i].second);
//         double querySim = cosineSimilarity(queryVector, movieVectors[i].second);

//         double topicSim = 0.0;
//         if (movieTopics.count(movieId))
//         {
//             topicSim = topicSimilarity(queryTopics, movieTopics[movieId]);
//         }

//         double finalScore =
//             alpha * userSim +
//             beta * querySim +
//             gamma * topicSim;

//         scores.push_back(make_pair(finalScore, movieId));
//     }

//     sort(scores.rbegin(), scores.rend());

//     vector<int> recs;
//     for (int i = 0; i < topN && i < (int)scores.size(); i++)
//     {
//         recs.push_back(scores[i].second);
//     }

//     return recs;
// }

unordered_map<int, vector<string>> loadMovieTopics(const string &file)
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
        {
            topics[id].push_back(word);
        }
    }

    in.close();
    return topics;
}

vector<string> processQuery(string query)
{
    return tokenize(query);
}

// Jaccard similarity for topic words
double topicSimilarity(const vector<string> &A, const vector<string> &B)
{
    if (A.empty() || B.empty())
    {
        return 0.0;
    }

    unordered_set<string> setA(A.begin(), A.end());
    unordered_set<string> setB(B.begin(), B.end());

    int intersection = 0;

    unordered_set<string>::const_iterator it;
    for (it = setB.begin(); it != setB.end(); ++it)
    {
        if (setA.count(*it))
        {
            intersection++;
        }
    }

    int unionSize = (int)setA.size() + (int)setB.size() - intersection;

    if (unionSize == 0)
    {
        return 0.0;
    }

    return (double)intersection / unionSize;
}
