#ifndef CBF_H
#define CBF_H

#include <unordered_map>
#include <vector>
#include <string>
#include <unordered_set>

using Vector = std::unordered_map<std::string, double>;

std::vector<std::pair<int, Vector>>
loadMovieVectors(const std::string &filename);

std::unordered_map<int, std::string>
loadMovieTitles(const std::string &csvFile);

std::unordered_map<int, std::vector<std::string>>
loadMovieTopics(const std::string &file);

std::unordered_set<int>
getRatedMovieIds(const std::string &ratingsFile, int userId);

Vector buildUserVector(
    const std::string &ratingsFile,
    const std::vector<std::pair<int, Vector>> &movieVectors,
    int userId);

Vector buildQueryVector(
    const std::string &query,
    const std::unordered_map<std::string, int> &df,
    int totalDocs);

std::vector<std::string> processQuery(std::string query);

double cosineSimilarity(const Vector &A, const Vector &B);

double topicSimilarity(
    const std::vector<std::string> &queryTopics,
    const std::vector<std::string> &movieTopics);

bool isColdStart(const Vector &userVector);

std::vector<int> coldStartRecommend(
    const std::vector<std::pair<int, Vector>> &movieVectors,
    const Vector &queryVector,
    int topN);

std::vector<int> recommendMovies(
    const Vector &userVector,
    const Vector &queryVector,
    const std::vector<std::pair<int, Vector>> &movieVectors,
    int topN,
    const std::unordered_set<int> &ratedMovies,
    const std::vector<std::string> &queryTopics);

#endif
