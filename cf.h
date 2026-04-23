#ifndef CF_H
#define CF_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

using RatingsMap = std::unordered_map<int, std::unordered_map<int, double>>;
RatingsMap loadAllRatings(const std::string &filename);
double pearsonSimilarity(int userA, int userB, const RatingsMap &ratings);

double predictRating(
    int userId,
    int movieId,
    const RatingsMap &ratings,
    int topK = 30);

std::unordered_map<int, double> getCFScores(
    int userId,
    const RatingsMap &ratings,
    const std::unordered_set<int> &ratedMovies);

#endif
