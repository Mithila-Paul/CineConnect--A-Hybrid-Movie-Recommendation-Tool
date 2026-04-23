#ifndef GP_H
#define GP_H

#include "cf.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

using GPRatingsMap = RatingsMap;

std::unordered_map<int, double> computeGP(
    const GPRatingsMap &ratingsMap,
    int userId,
    const std::unordered_set<int> &ratedMovies,
    double alpha = 0.15,
    int iterations = 3);

#endif
