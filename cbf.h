#ifndef CBF_H
#define CBF_H

#include <unordered_map>
#include <vector>
#include <string>
#include <unordered_set>

std::unordered_set<int>
getRatedMovieIds(const std::string &ratingsFile);

std::unordered_map<int, std::string>
loadMovieTitles(const std::string &csvFile);

// defining vectors to avoid compilation issues
using Vector = std::unordered_map<std::string, double>;

std::vector<int> coldStartRecommend(
    const std::vector<std::pair<int, Vector>> &movieVectors,
    const Vector &queryVector,
    int topN);

std::vector<std::pair<int, Vector>>
loadMovieVectors(const std::string &filename);

Vector buildUserVector(
    const std::string &ratingsFile,
    const std::vector<std::pair<int, Vector>> &movieVectors);

double cosineSimilarity(const Vector &A, const Vector &B);

std::unordered_map<int, std::vector<std::string>> loadMovieTopics(const std::string &file);

Vector buildQueryVector(
    const std::string &query,
    const std::unordered_map<std::string, int> &df,
    int totalDocs);

std::vector<int> recommendMovies(
    const Vector &userVector,
    const Vector &queryVector,
    const std::vector<std::pair<int, Vector>> &movieVectors,
    int topN,
    const std::unordered_set<int> &ratedMovies,
    const std::vector<std::string> &queryTopics // The 6th argument
);

double topicSimilarity(
    const std::vector<std::string> &queryTopics,
    const std::vector<std::string> &movieTopics);

#endif
