#include "cf.h"
#include "csv_parser.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

using namespace std;

RatingsMap loadAllRatings(const string &filename)
{
    RatingsMap ratings;
    ifstream file(filename.c_str());

    if (!file.is_open())
    {
        cout << "CF Error: cannot open " << filename << "\n";
        return ratings;
    }

    string line;
    getline(file, line); 

    while (getline(file, line))
    {
        stripCR(line);

        if (line.empty())
        {
            continue;
        }

        istringstream ss(line);
        string movieIdStr = parseCsvField(ss);
        string userIdStr = parseCsvField(ss);
        string ratingStr = parseCsvField(ss);

        if (movieIdStr.empty() || userIdStr.empty() || ratingStr.empty())
        {
            continue;
        }

        try
        {
            int movieId = stoi(movieIdStr);
            int userId = stoi(userIdStr);
            double rating = stod(ratingStr);

            ratings[userId][movieId] = rating;
        }
        catch (...)
        {
            continue;
        }
    }

    file.close();
    return ratings;
}

static double userMean(int userId, const RatingsMap &ratings)
{
    RatingsMap::const_iterator it = ratings.find(userId);

    if (it == ratings.end() || it->second.empty())
    {
        return 0.0;
    }

    double sum = 0.0;

    unordered_map<int, double>::const_iterator kv;
    for (kv = it->second.begin(); kv != it->second.end(); ++kv)
    {
        sum += kv->second;
    }

    return sum / (double)it->second.size();
}

double pearsonSimilarity(int userA, int userB, const RatingsMap &ratings)
{
    RatingsMap::const_iterator itA = ratings.find(userA);
    RatingsMap::const_iterator itB = ratings.find(userB);

    if (itA == ratings.end() || itB == ratings.end())
    {
        return 0.0;
    }

    const unordered_map<int, double> &ratingsA = itA->second;
    const unordered_map<int, double> &ratingsB = itB->second;

    double meanA = userMean(userA, ratings);
    double meanB = userMean(userB, ratings);

    double numerator = 0.0;
    double denomA = 0.0;
    double denomB = 0.0;
    int coRated = 0;

    const unordered_map<int, double> *smaller;
    const unordered_map<int, double> *larger;
    double meanSmaller;
    double meanLarger;

    if (ratingsA.size() <= ratingsB.size())
    {
        smaller = &ratingsA;
        larger = &ratingsB;
        meanSmaller = meanA;
        meanLarger = meanB;
    }
    else
    {
        smaller = &ratingsB;
        larger = &ratingsA;
        meanSmaller = meanB;
        meanLarger = meanA;
    }

    unordered_map<int, double>::const_iterator kv;
    for (kv = smaller->begin(); kv != smaller->end(); ++kv)
    {
        unordered_map<int, double>::const_iterator jt = larger->find(kv->first);

        if (jt == larger->end())
        {
            continue;
        }

        double devS = kv->second - meanSmaller;
        double devL = jt->second - meanLarger;

        numerator += devS * devL;
        denomA += devS * devS;
        denomB += devL * devL;
        coRated++;
    }

    if (coRated < 2 || denomA == 0.0 || denomB == 0.0)
    {
        return 0.0;
    }

    double rawSim = numerator / (sqrt(denomA) * sqrt(denomB));

    const int SHRINKAGE_K = 10;
    double significance = (double)coRated / (coRated + SHRINKAGE_K);

    return rawSim * significance;
}

static pair<double, double> neighborWeightedDeviation(
    int movieId,
    const vector<pair<double, int>> &neighbors,
    const RatingsMap &ratings,
    const unordered_map<int, double> &neighborMeans)
{
    double num = 0.0;
    double den = 0.0;

    vector<pair<double, int>>::const_iterator sv;
    for (sv = neighbors.begin(); sv != neighbors.end(); ++sv)
    {
        int nId = sv->second;
        double sim = sv->first;

        unordered_map<int, double>::const_iterator it = ratings.at(nId).find(movieId);
        if (it == ratings.at(nId).end())
        {
            continue;
        }

        double meanV = neighborMeans.at(nId);
        double ratingVm = it->second;

        num += sim * (ratingVm - meanV);
        den += sim;
    }

    return pair<double, double>(num, den);
}

bool comparePairByFirstDesc(const pair<double, int> &a, const pair<double, int> &b)
{
    return a.first > b.first;
}

double predictRating(
    int userId,
    int movieId,
    const RatingsMap &ratings,
    int topK)
{
    double meanU = userMean(userId, ratings);

    vector<pair<double, int>> neighbors;

    RatingsMap::const_iterator kv;
    for (kv = ratings.begin(); kv != ratings.end(); ++kv)
    {
        int neighborId = kv->first;

        if (neighborId == userId)
        {
            continue;
        }

        if (kv->second.find(movieId) == kv->second.end())
        {
            continue;
        }

        double sim = pearsonSimilarity(userId, neighborId, ratings);

        if (sim > 0.0)
        {
            neighbors.push_back(pair<double, int>(sim, neighborId));
        }
    }

    if (neighbors.empty())
    {
        return meanU;
    }

    if ((int)neighbors.size() > topK)
    {
        partial_sort(
            neighbors.begin(),
            neighbors.begin() + topK,
            neighbors.end(),
            comparePairByFirstDesc);

        neighbors.resize(topK);
    }

    unordered_map<int, double> neighborMeans;

    vector<pair<double, int>>::const_iterator sv;
    for (sv = neighbors.begin(); sv != neighbors.end(); ++sv)
    {
        neighborMeans[sv->second] = userMean(sv->second, ratings);
    }

    pair<double, double> result =
        neighborWeightedDeviation(movieId, neighbors, ratings, neighborMeans);

    double num = result.first;
    double den = result.second;

    if (den == 0.0)
    {
        return meanU;
    }

    double predicted = meanU + (num / den);

    if (predicted < 0.5)
    {
        predicted = 0.5;
    }

    if (predicted > 5.0)
    {
        predicted = 5.0;
    }

    return predicted;
}

unordered_map<int, double> getCFScores(
    int userId,
    const RatingsMap &ratings,
    const unordered_set<int> &ratedMovies)
{
    unordered_set<int> allMovies;

    RatingsMap::const_iterator userKV;
    for (userKV = ratings.begin(); userKV != ratings.end(); ++userKV)
    {
        unordered_map<int, double>::const_iterator movieKV;
        for (movieKV = userKV->second.begin(); movieKV != userKV->second.end(); ++movieKV)
        {
            allMovies.insert(movieKV->first);
        }
    }

    vector<pair<double, int>> simCache;
    simCache.reserve(ratings.size());

    RatingsMap::const_iterator kv;
    for (kv = ratings.begin(); kv != ratings.end(); ++kv)
    {
        int nId = kv->first;

        if (nId == userId)
        {
            continue;
        }

        double sim = pearsonSimilarity(userId, nId, ratings);

        if (sim > 0.0)
        {
            simCache.push_back(pair<double, int>(sim, nId));
        }
    }

    sort(simCache.begin(), simCache.end(), comparePairByFirstDesc);

    const int MAX_NEIGHBORS = 50;
    if ((int)simCache.size() > MAX_NEIGHBORS)
    {
        simCache.resize(MAX_NEIGHBORS);
    }

    double meanU = userMean(userId, ratings);

    unordered_map<int, double> neighborMeans;
    vector<pair<double, int>>::const_iterator sv;
    for (sv = simCache.begin(); sv != simCache.end(); ++sv)
    {
        neighborMeans[sv->second] = userMean(sv->second, ratings);
    }

    unordered_map<int, double> cfScores;

    unordered_set<int>::const_iterator movieIt;
    for (movieIt = allMovies.begin(); movieIt != allMovies.end(); ++movieIt)
    {
        int movieId = *movieIt;

        if (ratedMovies.find(movieId) != ratedMovies.end())
        {
            continue;
        }

        pair<double, double> result =
            neighborWeightedDeviation(movieId, simCache, ratings, neighborMeans);

        double num = result.first;
        double den = result.second;
        double predicted;

        if (den == 0.0)
        {
            predicted = meanU;
        }
        else
        {
            predicted = meanU + (num / den);
        }

        if (predicted < 0.5)
        {
            predicted = 0.5;
        }

        if (predicted > 5.0)
        {
            predicted = 5.0;
        }

        cfScores[movieId] = predicted / 5.0;
    }

    return cfScores;
}
