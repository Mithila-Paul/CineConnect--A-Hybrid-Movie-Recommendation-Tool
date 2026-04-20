#include "cf.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

using namespace std;

// =============================================================================
// loadAllRatings
// Reads ratings_processed.csv (format: movieId,userId,rating) into memory.
// Stored as ratings[userId][movieId] = rating for O(1) access.
// Call this ONCE and reuse — 53K rows loads in ~50ms.
// =============================================================================
RatingsMap loadAllRatings(const string &filename)
{
    RatingsMap ratings;
    ifstream file(filename);

    if (!file.is_open())
    {
        cout << "CF Error: cannot open " << filename << "\n";
        return ratings;
    }

    string line;
    getline(file, line); // skip header: movieId,userId,rating

    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back(); // strip Windows \r

        stringstream ss(line);
        string movieIdStr, userIdStr, ratingStr;

        getline(ss, movieIdStr, ',');
        getline(ss, userIdStr,  ',');
        getline(ss, ratingStr,  ',');

        if (movieIdStr.empty() || userIdStr.empty() || ratingStr.empty())
            continue;

        try
        {
            int    movieId = stoi(movieIdStr);
            int    userId  = stoi(userIdStr);
            double rating  = stod(ratingStr);
            ratings[userId][movieId] = rating;
        }
        catch (...) { continue; }
    }

    file.close();
    return ratings;
}

// =============================================================================
// userMean  (internal helper)
// Computes average rating for a user across all movies they rated.
// Used to center ratings in Pearson and in the prediction formula.
// =============================================================================
static double userMean(int userId, const RatingsMap &ratings)
{
    auto it = ratings.find(userId);
    if (it == ratings.end() || it->second.empty())
        return 0.0;

    double sum = 0.0;
    for (auto &kv : it->second)
        sum += kv.second;

    return sum / (double)it->second.size();
}

// =============================================================================
// pearsonSimilarity
//
// Measures how similarly two users rate movies they have BOTH seen.
//
// Formula:
//   sim(A,B) =        Σ (rA,i - meanA)(rB,i - meanB)
//              ─────────────────────────────────────────────────────
//              sqrt( Σ(rA,i - meanA)² ) × sqrt( Σ(rB,i - meanB)² )
//
// Where i ranges over movies rated by BOTH A and B (co-rated set).
//
// Why Pearson over cosine here?
//   Cosine treats unrated movies as 0, which biases generous raters
//   (who tend to rate 4-5) vs harsh raters (who tend to rate 2-3).
//   Pearson works in deviations from each user's mean, so those
//   personal rating biases cancel out.
//
// Returns 0 if:
//   - either user not found
//   - fewer than 2 co-rated movies (1 shared movie gives trivial ±1, noise)
//   - zero variance in either user's ratings (all same rating = no signal)
// =============================================================================
double pearsonSimilarity(int userA, int userB, const RatingsMap &ratings)
{
    auto itA = ratings.find(userA);
    auto itB = ratings.find(userB);
    if (itA == ratings.end() || itB == ratings.end()) return 0.0;

    const auto &ratingsA = itA->second;
    const auto &ratingsB = itB->second;

    double meanA = userMean(userA, ratings);
    double meanB = userMean(userB, ratings);

    double numerator = 0.0;
    double denomA    = 0.0;
    double denomB    = 0.0;
    int    coRated   = 0;

    // Iterate the smaller map, look up in the larger — avoids O(n²) inner loop
    const auto &smaller     = (ratingsA.size() <= ratingsB.size()) ? ratingsA : ratingsB;
    const auto &larger      = (ratingsA.size() <= ratingsB.size()) ? ratingsB : ratingsA;
    double      meanSmaller = (ratingsA.size() <= ratingsB.size()) ? meanA    : meanB;
    double      meanLarger  = (ratingsA.size() <= ratingsB.size()) ? meanB    : meanA;

    for (auto &kv : smaller)
    {
        auto jt = larger.find(kv.first);
        if (jt == larger.end()) continue; // not co-rated, skip

        double devS = kv.second  - meanSmaller;
        double devL = jt->second - meanLarger;

        numerator += devS * devL;
        denomA    += devS * devS;
        denomB    += devL * devL;
        coRated++;
    }

    if (coRated < 2 || denomA == 0.0 || denomB == 0.0)
        return 0.0;

    return numerator / (sqrt(denomA) * sqrt(denomB));
}

// =============================================================================
// predictRating
//
// Predicts the rating user u would give movie m using their K most similar
// neighbors who have actually rated movie m.
//
// Formula (weighted deviation from neighbor mean):
//   pred(u,m) = mean_u  +  Σ [ sim(u,v) × (r(v,m) − mean_v) ]
//                           ──────────────────────────────────
//                                    Σ |sim(u,v)|
//
// Why subtract neighbor mean (r(v,m) − mean_v)?
//   Same reason as Pearson — corrects for rating bias. A generous
//   neighbor who rates everything 4.5 giving movie m a 4.0 is actually
//   a BELOW average signal. The deviation tells us what's truly special.
//
// Only POSITIVE similarities are used as neighbors. A negatively
// correlated user liking a movie is a signal you WON'T like it — but
// including negative sims in the formula requires taking abs() in the
// denominator, which makes the math more complex. Using only positive
// neighbors is standard and works well in practice.
//
// Falls back to user's own mean if no neighbor has rated movie m.
// Result is clamped to [0.5, 5.0] — the valid rating range.
// =============================================================================
double predictRating(int userId, int movieId,
                     const RatingsMap &ratings,
                     int topK)
{
    double meanU = userMean(userId, ratings);

    // Find all neighbors who (a) have positive Pearson sim and (b) rated this movie
    vector<pair<double, int>> neighbors; // (sim, neighborId)

    for (auto &kv : ratings)
    {
        int neighborId = kv.first;
        if (neighborId == userId) continue;
        if (kv.second.find(movieId) == kv.second.end()) continue; // hasn't rated m

        double sim = pearsonSimilarity(userId, neighborId, ratings);
        if (sim > 0.0)
            neighbors.push_back({sim, neighborId});
    }

    if (neighbors.empty())
        return meanU; // no useful neighbors for this movie

    // Keep only topK most similar neighbors
    if ((int)neighbors.size() > topK)
    {
        partial_sort(neighbors.begin(), neighbors.begin() + topK, neighbors.end(),
                     [](auto &a, auto &b){ return a.first > b.first; });
        neighbors.resize(topK);
    }

    double num = 0.0, den = 0.0;
    for (auto &sv : neighbors)
    {
        double sim      = sv.first;
        int    nId      = sv.second;
        double meanV    = userMean(nId, ratings);
        double ratingVm = ratings.at(nId).at(movieId);

        num += sim * (ratingVm - meanV);
        den += sim;
    }

    if (den == 0.0) return meanU;

    double predicted = meanU + (num / den);
    return max(0.5, min(5.0, predicted)); // clamp to valid range
}

// =============================================================================
// getCFScores
//
// Builds a complete movieId -> predicted_rating map for every movie
// the user has NOT yet rated. This is what gets passed to the hybrid
// scorer in recommendMoviesForUser so it can look up CF score in O(1)
// per movie instead of calling predictRating inside a loop (which would
// recompute all neighbors each time).
//
// Scores are normalized to [0, 1] by dividing by 5.0 so they can be
// blended with CBF cosine similarity scores (which are already in [0,1]).
// =============================================================================
unordered_map<int, double> getCFScores(
    int userId,
    const RatingsMap &ratings,
    const unordered_set<int> &ratedMovies)
{
    // Collect all movies that appear anywhere in the dataset
    unordered_set<int> allMovies;
    for (auto &userKV : ratings)
        for (auto &movieKV : userKV.second)
            allMovies.insert(movieKV.first);

    // Precompute all neighbor similarities once (not per-movie)
    // sim_cache[neighborId] = pearson sim with userId
    vector<pair<double, int>> simCache; // (sim, neighborId)
    simCache.reserve(ratings.size());

    for (auto &kv : ratings)
    {
        int nId = kv.first;
        if (nId == userId) continue;
        double sim = pearsonSimilarity(userId, nId, ratings);
        if (sim > 0.0)
            simCache.push_back({sim, nId});
    }

    // Sort descending so topK selection is just resize
    sort(simCache.rbegin(), simCache.rend());

    // Cap at top 50 neighbors globally for speed
    const int MAX_NEIGHBORS = 50;
    if ((int)simCache.size() > MAX_NEIGHBORS)
        simCache.resize(MAX_NEIGHBORS);

    double meanU = userMean(userId, ratings);

    // Cache neighbor means once — avoids recomputing per movie per neighbor
    // Without this, userMean is called (50 neighbors × 2701 movies) = 135,050 times
    // With cache it is called 50 times total
    unordered_map<int, double> neighborMeans;
    for (auto &sv : simCache)
        neighborMeans[sv.second] = userMean(sv.second, ratings);

    unordered_map<int, double> cfScores;

    for (int movieId : allMovies)
    {
        if (ratedMovies.count(movieId)) continue; // skip already rated

        double num = 0.0, den = 0.0;

        for (auto &sv : simCache)
        {
            int    nId = sv.second;
            double sim = sv.first;

            auto it = ratings.at(nId).find(movieId);
            if (it == ratings.at(nId).end()) continue; // neighbor hasn't rated this movie

            double meanV    = neighborMeans.at(nId); // O(1) lookup, not recomputed
            double ratingVm = it->second;

            num += sim * (ratingVm - meanV);
            den += sim;
        }

        double predicted;
        if (den == 0.0)
            predicted = meanU; // no neighbor rated this — fall back to user mean
        else
            predicted = meanU + (num / den);

        predicted = max(0.5, min(5.0, predicted));

        // Normalize to [0,1] for blending with CBF scores
        cfScores[movieId] = predicted / 5.0;
    }

    return cfScores;
}