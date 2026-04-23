#ifndef MF_H
#define MF_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct MFModel
{
    std::vector<std::vector<double>> P; // [numUsers][K]
    std::vector<std::vector<double>> Q; // [numMovies][K]

    std::vector<double> userBias;  // bu
    std::vector<double> movieBias; // bm
    double globalMean;             // mean of all ratings

    std::unordered_map<int, int> userIndex;
    std::unordered_map<int, int> movieIndex;

    std::vector<int> indexToUser;
    std::vector<int> indexToMovie;

    bool trained = false;
};

MFModel trainMF(
    const std::string &ratingsFile,
    int K = 30,
    int epochs = 40,
    double lr = 0.005,
    double lambda = 0.02);

void saveMFModel(const MFModel &model, const std::string &filename);

MFModel loadMFModel(const std::string &filename);

double mfPredict(int userId, int movieId, const MFModel &model);

std::unordered_map<int, double> getMFScores(
    int userId,
    const MFModel &model,
    const std::unordered_set<int> &ratedMovies);

MFModel initMF(const std::string &ratingsFile);

#endif
