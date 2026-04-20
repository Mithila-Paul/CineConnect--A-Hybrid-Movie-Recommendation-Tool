#ifndef CF_H
#define CF_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

// ratings[userId][movieId] = rating
using RatingsMap = std::unordered_map<int, std::unordered_map<int, double>>;

// Load entire ratings_processed.csv into memory once.
// Call once in recommendMoviesForUser, pass it down — never reload per call.
RatingsMap loadAllRatings(const std::string &filename);

// Pearson correlation between two users [-1, 1].
// Only co-rated movies count. Returns 0 if fewer than 2 co-rated movies.
double pearsonSimilarity(int userA, int userB, const RatingsMap &ratings);

// Predict the rating user would give movieId using top-K neighbors.
// Returns user mean if no neighbor has rated that movie (graceful fallback).
double predictRating(int userId, int movieId,
                     const RatingsMap &ratings,
                     int topK = 30);

// Returns movieId -> predicted rating for ALL unrated movies.
// Used by the hybrid scorer for O(1) CF score lookup per movie.
std::unordered_map<int, double> getCFScores(
    int userId,
    const RatingsMap &ratings,
    const std::unordered_set<int> &ratedMovies);

#endif