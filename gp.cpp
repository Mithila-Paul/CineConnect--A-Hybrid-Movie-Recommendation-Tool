#include "gp.h"
#include <cmath>
#include <algorithm>
#include <numeric>

using namespace std;

unordered_map<int, double> computeGP(
    const GPRatingsMap &ratingsMap,
    int userId,
    const unordered_set<int> &ratedMovies,
    double alpha,
    int iterations)
{
    unordered_map<int, vector<pair<int, double>>> edges;
    unordered_map<int, int> movieDegree;

    GPRatingsMap::const_iterator it;
    for (it = ratingsMap.begin(); it != ratingsMap.end(); ++it)
    {
        int uid = it->first;
        const unordered_map<int, double> &movieRatings = it->second;
        int uNode = uid;

        unordered_map<int, double>::const_iterator jt;
        for (jt = movieRatings.begin(); jt != movieRatings.end(); ++jt)
        {
            int mid = jt->first;
            double rating = jt->second;
            int mNode = -mid;

            double uToM = rating / 5.0;
            edges[uNode].push_back(make_pair(mNode, uToM));

            edges[mNode].push_back(make_pair(uNode, 1.0));
            movieDegree[mNode]++;
        }
    }

    unordered_map<int, vector<pair<int, double>>>::iterator eit;
    for (eit = edges.begin(); eit != edges.end(); ++eit)
    {
        int src = eit->first;
        vector<pair<int, double>> &adjList = eit->second;

        if (src >= 0)
        {
            continue;
        }

        int degree = 1;
        unordered_map<int, int>::iterator degIt = movieDegree.find(src);

        if (degIt != movieDegree.end())
        {
            degree = degIt->second;
        }

        for (int i = 0; i < (int)adjList.size(); i++)
        {
            adjList[i].second = 1.0 / degree;
        }
    }

    for (eit = edges.begin(); eit != edges.end(); ++eit)
    {
        int src = eit->first;
        vector<pair<int, double>> &adjList = eit->second;

        if (src < 0)
        {
            continue;
        }

        double outSum = 0.0;

        for (int i = 0; i < (int)adjList.size(); i++)
        {
            outSum += adjList[i].second;
        }

        if (outSum > 0.0)
        {
            for (int i = 0; i < (int)adjList.size(); i++)
            {
                adjList[i].second /= outSum;
            }
        }
    }

    unordered_map<int, double> seed;

    GPRatingsMap::const_iterator uitr = ratingsMap.find(userId);
    if (uitr == ratingsMap.end())
    {
        return unordered_map<int, double>();
    }

    double seedSum = 0.0;
    const unordered_map<int, double> &userRatings = uitr->second;

    unordered_map<int, double>::const_iterator sit;
    for (sit = userRatings.begin(); sit != userRatings.end(); ++sit)
    {
        int mid = sit->first;
        double rating = sit->second;

        if (rating <= 0.0)
        {
            continue;
        }

        seed[-mid] = rating / 5.0;
        seedSum += rating / 5.0;
    }

    if (seedSum == 0.0)
    {
        return unordered_map<int, double>();
    }

    unordered_map<int, double>::iterator seedIt;
    for (seedIt = seed.begin(); seedIt != seed.end(); ++seedIt)
    {
        seedIt->second /= seedSum;
    }

    unordered_map<int, double> score = seed;

    for (int iter = 0; iter < iterations; iter++)
    {
        unordered_map<int, double> newScore;

        unordered_map<int, double>::const_iterator seedConstIt;
        for (seedConstIt = seed.begin(); seedConstIt != seed.end(); ++seedConstIt)
        {
            int node = seedConstIt->first;
            double s = seedConstIt->second;
            newScore[node] += alpha * s;
        }

        unordered_map<int, vector<pair<int, double>>>::const_iterator edgeIt;
        for (edgeIt = edges.begin(); edgeIt != edges.end(); ++edgeIt)
        {
            int src = edgeIt->first;
            const vector<pair<int, double>> &adjList = edgeIt->second;

            double srcScore = 0.0;
            unordered_map<int, double>::const_iterator scIt = score.find(src);

            if (scIt != score.end())
            {
                srcScore = scIt->second;
            }

            if (srcScore == 0.0)
            {
                continue;
            }

            for (int i = 0; i < (int)adjList.size(); i++)
            {
                int dst = adjList[i].first;
                double w = adjList[i].second;
                newScore[dst] += (1.0 - alpha) * srcScore * w;
            }
        }

        score = newScore;
    }

    unordered_map<int, double> gpScores;
    double maxScore = 0.0;

    unordered_map<int, double>::const_iterator finalIt;
    for (finalIt = score.begin(); finalIt != score.end(); ++finalIt)
    {
        int node = finalIt->first;
        double s = finalIt->second;

        if (node >= 0)
        {
            continue;
        }

        int movieId = -node;

        if (ratedMovies.find(movieId) != ratedMovies.end())
        {
            continue;
        }

        gpScores[movieId] = s;

        if (s > maxScore)
        {
            maxScore = s;
        }
    }

    if (maxScore > 0.0)
    {
        unordered_map<int, double>::iterator normIt;
        for (normIt = gpScores.begin(); normIt != gpScores.end(); ++normIt)
        {
            normIt->second /= maxScore;
        }
    }

    return gpScores;
}
