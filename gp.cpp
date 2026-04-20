#include "gp.h"
#include <cmath>
#include <algorithm>
#include <numeric>

using namespace std;

// =============================================================================
// computeGP
//
// Full implementation notes
// ──────────────────────────
//
// Node encoding
//   Users  are stored with key  +userId   (positive int)
//   Movies are stored with key  -movieId  (negative int)
// This lets a single unordered_map<int,double> hold both kinds of node
// without any struct overhead or separate maps.
//
// Graph construction  (O(R) where R = number of ratings)
//   user→movie edges:  w(u→m) = rating(u,m) / 5.0
//   movie→user edges:  w(m→u) = 1 / degree(m)
//
// Seed vector
//   seed(-movieId) = normalised_rating / sum_of_normalised_ratings
//
// Propagation
//   score_new(v) = α · seed(v)  +  (1-α) · Σ_u [ score(u) · w(u→v) ]
//
// Output
//   Only movie nodes (negative keys) that are NOT in ratedMovies are
//   returned, normalised to [0,1].
// =============================================================================

unordered_map<int, double> computeGP(
    const GPRatingsMap &ratingsMap,
    int userId,
    const unordered_set<int> &ratedMovies,
    double alpha,
    int iterations)
{
    // ── 1. Build adjacency list ───────────────────────────────────────────────
    unordered_map<int, vector<pair<int, double> > > edges;
    unordered_map<int, int> movieDegree; // key = -movieId

    for (GPRatingsMap::const_iterator it = ratingsMap.begin(); it != ratingsMap.end(); ++it)
    {
        int uid = it->first;
        const unordered_map<int, double> &movieRatings = it->second;
        int uNode = uid; // positive

        for (unordered_map<int, double>::const_iterator jt = movieRatings.begin(); jt != movieRatings.end(); ++jt)
        {
            int mid = jt->first;
            double rating = jt->second;
            int mNode = -mid; // negative

            // user -> movie edge
            double uToM = rating / 5.0;
            edges[uNode].push_back(make_pair(mNode, uToM));

            // movie -> user placeholder
            edges[mNode].push_back(make_pair(uNode, 1.0));
            movieDegree[mNode]++;
        }
    }

    // Normalize movie -> user edge weights by movie degree
    for (unordered_map<int, vector<pair<int, double> > >::iterator it = edges.begin(); it != edges.end(); ++it)
    {
        int src = it->first;
        vector<pair<int, double> > &adjList = it->second;

        if (src >= 0) continue; // skip user nodes

        int degree = 1;
        unordered_map<int, int>::iterator degIt = movieDegree.find(src);
        if (degIt != movieDegree.end())
            degree = degIt->second;

        for (size_t i = 0; i < adjList.size(); i++)
            adjList[i].second = 1.0 / degree;
    }

    // ── 2. Build seed vector ──────────────────────────────────────────────────
    unordered_map<int, double> seed;

    GPRatingsMap::const_iterator uitr = ratingsMap.find(userId);
    if (uitr == ratingsMap.end())
        return unordered_map<int, double>(); // no ratings for this user

    double seedSum = 0.0;
    const unordered_map<int, double> &userRatings = uitr->second;

    for (unordered_map<int, double>::const_iterator it = userRatings.begin(); it != userRatings.end(); ++it)
    {
        int mid = it->first;
        double rating = it->second;

        if (rating <= 0.0) continue;

        seed[-mid] = rating / 5.0;
        seedSum += rating / 5.0;
    }

    if (seedSum == 0.0)
        return unordered_map<int, double>();

    for (unordered_map<int, double>::iterator it = seed.begin(); it != seed.end(); ++it)
        it->second /= seedSum;

    // ── 3. Initialise score vector = seed ─────────────────────────────────────
    unordered_map<int, double> score = seed;

    // ── 4. Propagation iterations ─────────────────────────────────────────────
    for (int iter = 0; iter < iterations; iter++)
    {
        unordered_map<int, double> newScore;

        // Teleport term
        for (unordered_map<int, double>::const_iterator it = seed.begin(); it != seed.end(); ++it)
        {
            int node = it->first;
            double s = it->second;
            newScore[node] += alpha * s;
        }

        // Propagation term
        for (unordered_map<int, vector<pair<int, double> > >::const_iterator it = edges.begin(); it != edges.end(); ++it)
        {
            int src = it->first;
            const vector<pair<int, double> > &adjList = it->second;

            double srcScore = 0.0;
            unordered_map<int, double>::const_iterator scIt = score.find(src);
            if (scIt != score.end())
                srcScore = scIt->second;

            if (srcScore == 0.0) continue;

            for (size_t i = 0; i < adjList.size(); i++)
            {
                int dst = adjList[i].first;
                double w = adjList[i].second;
                newScore[dst] += (1.0 - alpha) * srcScore * w;
            }
        }

        score = newScore;
    }

    // ── 5. Extract and normalise movie scores ─────────────────────────────────
    unordered_map<int, double> gpScores;
    double maxScore = 0.0;

    for (unordered_map<int, double>::const_iterator it = score.begin(); it != score.end(); ++it)
    {
        int node = it->first;
        double s = it->second;

        if (node >= 0) continue; // skip user nodes

        int movieId = -node;
        if (ratedMovies.find(movieId) != ratedMovies.end()) continue;

        gpScores[movieId] = s;
        if (s > maxScore) maxScore = s;
    }

    if (maxScore > 0.0)
    {
        for (unordered_map<int, double>::iterator it = gpScores.begin(); it != gpScores.end(); ++it)
            it->second /= maxScore;
    }

    return gpScores;
}