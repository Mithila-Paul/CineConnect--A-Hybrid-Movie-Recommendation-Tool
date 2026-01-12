#include "cbf.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace std;

//to load the movie vectors from file
vector<pair<int, Vector>> loadMovieVectors(const string& filename) {
    ifstream file(filename);
    vector<pair<int, Vector>> movieVectors;
     if (!file.is_open()) {
        cout << "Error opening movie vectors file\n";
        return movieVectors;
        }

    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        int movieID;
        ss >> movieID;

        Vector vec;
        string token;
        while (ss >> token) {
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

// to add cold start recommendation, currently not functionable
vector<int> coldStartRecommend(
    const vector<pair<int, Vector>>& movieVectors,
    int topN
) {
    vector<pair<double, int>> scores;

    for (auto& mv : movieVectors) {
        double norm = 0.0;
        for (auto& it : mv.second)
            norm += it.second * it.second;

        norm = sqrt(norm);
        scores.push_back({norm, mv.first});
    }

    sort(scores.rbegin(), scores.rend());

    vector<int> recommendations;
    for (int i = 0; i < topN && i < scores.size(); i++)
        recommendations.push_back(scores[i].second);

    return recommendations;
}

// detect cold start :, currently not functionable
bool isColdStart(const Vector& userVector) {
    return userVector.empty();
}


//building user preference vector
Vector buildUserVector(
    const string& ratingsFile,
    const vector<pair<int, Vector>>& movieVectors)
    {
    ifstream file(ratingsFile);
    Vector userVector;

        if (!file.is_open()) {
            cout << "Error opening user ratings file\n";
            return userVector;
        }

    int movieID;
    double rating;

        while (file >> movieID >> rating) {
            for (auto& mv : movieVectors) {
                if (mv.first == movieID) {
                    for (auto& it : mv.second) {
                        userVector[it.first] += rating * it.second;
                    }
                    break;
                }
            }
        }

    // Normalizing user vector
    double norm = 0.0;
    for (auto& it : userVector)
        norm += it.second * it.second;

    norm = sqrt(norm);
    if (norm > 0) {
        for (auto& it : userVector)
            it.second /= norm;
    }

    return userVector;
}
// to get already rated movie IDs so that I can exclude them from recommendations
unordered_set<int>
getRatedMovieIds(const string& ratingsFile) {
    unordered_set<int> rated;
    ifstream in(ratingsFile);

    int movieId;
    double rating;

    while (in >> movieId >> rating)
        rated.insert(movieId);

    return rated;
}

// load movie titles from CSV to map them with ids
unordered_map<int, string>
loadMovieTitles(const string& csvFile) {
    unordered_map<int, string> titles;
    ifstream file(csvFile);

    string line;
    getline(file, line); // header

    while (getline(file, line)) {
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
double cosineSimilarity(const Vector& A, const Vector& B) {
    double dot = 0.0, normA = 0.0, normB = 0.0;

    for (auto& it : A) {
        normA += it.second * it.second;
        if (B.count(it.first))
            dot += it.second * B.at(it.first);
    }

    for (auto& it : B)
        normB += it.second * it.second;

    if (normA == 0 || normB == 0)
        return 0.0;

    return dot / (sqrt(normA) * sqrt(normB));
}

// cbf recommendation function on basis of users personal ratings, suggesting top 10 movies 
vector<int> recommendMovies(
    const Vector& userVector,
    const vector<pair<int, Vector>>& movieVectors,
    int topN, const unordered_set<int>& ratedMovies
) {
    vector<pair<double, int>> scores;

    for (auto& mv : movieVectors) {
        int movieId = mv.first;
        if (ratedMovies.count(movieId)) continue;  // this excludes already rated movies
        double sim = cosineSimilarity(userVector, mv.second);
        scores.push_back({sim, movieId});
    }

    sort(scores.rbegin(), scores.rend());

    vector<int> recommendations;
    for (int i = 0; i < topN && i < scores.size(); i++)
        recommendations.push_back(scores[i].second);

    return recommendations;
}
