#include "mf.h"
#include "csv_parser.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <random>
#include <numeric>
#include <cstring>
#include <sys/stat.h>

using namespace std;

struct RatingEntry
{
    int userId;
    int movieId;
    double rating;
};

static vector<RatingEntry> loadRatingsForMF(
    const string &filename,
    unordered_map<int, int> &userIndex,
    unordered_map<int, int> &movieIndex,
    vector<int> &indexToUser,
    vector<int> &indexToMovie)
{
    vector<RatingEntry> entries;

    ifstream file(filename);
    if (!file.is_open())
    {
        cout << "MF: cannot open " << filename << "\n";
        return entries;
    }

    string line;
    getline(file, line); // skip header

    while (getline(file, line))
    {
        stripCR(line);

        if (line.empty())
        {
            continue;
        }

        istringstream ss(line);
        string mStr = parseCsvField(ss);
        string uStr = parseCsvField(ss);
        string rStr = parseCsvField(ss);

        if (mStr.empty() || uStr.empty() || rStr.empty())
        {
            continue;
        }

        try
        {
            int movieId = stoi(mStr);
            int userId = stoi(uStr);
            double rating = stod(rStr);

            if (!userIndex.count(userId))
            {
                userIndex[userId] = (int)indexToUser.size();
                indexToUser.push_back(userId);
            }

            if (!movieIndex.count(movieId))
            {
                movieIndex[movieId] = (int)indexToMovie.size();
                indexToMovie.push_back(movieId);
            }

            RatingEntry entry;
            entry.userId = userId;
            entry.movieId = movieId;
            entry.rating = rating;

            entries.push_back(entry);
        }
        catch (...)
        {
            continue;
        }
    }

    file.close();
    return entries;
}

MFModel trainMF(
    const string &ratingsFile,
    int K,
    int epochs,
    double lr,
    double lambda)
{
    MFModel model;
    model.globalMean = 0.0;

    cout << "MF: loading ratings...\n";

    vector<RatingEntry> entries = loadRatingsForMF(
        ratingsFile,
        model.userIndex,
        model.movieIndex,
        model.indexToUser,
        model.indexToMovie);

    if (entries.empty())
    {
        cout << "MF: no ratings loaded\n";
        return model;
    }

    int numUsers = (int)model.indexToUser.size();
    int numMovies = (int)model.indexToMovie.size();

    cout << "MF: " << numUsers << " users, " << numMovies
         << " movies, " << entries.size() << " ratings\n";

    cout << "MF: training K=" << K << " factors, " << epochs << " epochs...\n";

    double sum = 0.0;
    for (int i = 0; i < (int)entries.size(); i++)
    {
        sum += entries[i].rating;
    }

    model.globalMean = sum / entries.size();

    mt19937 rng(42);
    uniform_real_distribution<double> dist(0.0, 0.1);

    model.P.assign(numUsers, vector<double>(K));
    model.Q.assign(numMovies, vector<double>(K));
    model.userBias.assign(numUsers, 0.0);
    model.movieBias.assign(numMovies, 0.0);

    for (int i = 0; i < (int)model.P.size(); i++)
    {
        for (int j = 0; j < (int)model.P[i].size(); j++)
        {
            model.P[i][j] = dist(rng);
        }
    }

    for (int i = 0; i < (int)model.Q.size(); i++)
    {
        for (int j = 0; j < (int)model.Q[i].size(); j++)
        {
            model.Q[i][j] = dist(rng);
        }
    }

    vector<int> order(entries.size());
    iota(order.begin(), order.end(), 0);

    for (int epoch = 0; epoch < epochs; epoch++)
    {
        shuffle(order.begin(), order.end(), rng);

        double epochLoss = 0.0;

        for (int i = 0; i < (int)order.size(); i++)
        {
            int idx = order[i];
            RatingEntry &e = entries[idx];

            int u = model.userIndex.at(e.userId);
            int m = model.movieIndex.at(e.movieId);

            double pred = model.globalMean + model.userBias[u] + model.movieBias[m];

            for (int k = 0; k < K; k++)
            {
                pred += model.P[u][k] * model.Q[m][k];
            }

            pred = max(0.5, min(5.0, pred));

            double err = e.rating - pred;
            epochLoss += err * err;

            model.userBias[u] += lr * (err - lambda * model.userBias[u]);
            model.movieBias[m] += lr * (err - lambda * model.movieBias[m]);

            vector<double> oldPu = model.P[u];

            for (int k = 0; k < K; k++)
            {
                model.P[u][k] += lr * (err * model.Q[m][k] - lambda * model.P[u][k]);
                model.Q[m][k] += lr * (err * oldPu[k] - lambda * model.Q[m][k]);
            }
        }

        double rmse = sqrt(epochLoss / entries.size());

        // cout << "  Epoch " << (epoch + 1) << "/" << epochs
        //      << "  RMSE: " << rmse << "\n";
    }

    model.trained = true;
    cout << "MF: training complete.\n";

    return model;
}

void saveMFModel(const MFModel &model, const string &filename)
{
    ofstream out(filename, ios::binary);

    if (!out.is_open())
    {
        cout << "MF: cannot save model\n";
        return;
    }

    int K = (int)model.P[0].size();
    int numUsers = (int)model.P.size();
    int numMovies = (int)model.Q.size();

    out.write((char *)&K, sizeof(int));
    out.write((char *)&numUsers, sizeof(int));
    out.write((char *)&numMovies, sizeof(int));
    out.write((char *)&model.globalMean, sizeof(double));

    for (int i = 0; i < (int)model.P.size(); i++)
    {
        out.write((char *)model.P[i].data(), K * sizeof(double));
    }

    for (int i = 0; i < (int)model.Q.size(); i++)
    {
        out.write((char *)model.Q[i].data(), K * sizeof(double));
    }

    out.write((char *)model.userBias.data(), numUsers * sizeof(double));
    out.write((char *)model.movieBias.data(), numMovies * sizeof(double));

    unordered_map<int, int>::const_iterator it;
    for (it = model.userIndex.begin(); it != model.userIndex.end(); ++it)
    {
        out.write((char *)&it->first, sizeof(int));
        out.write((char *)&it->second, sizeof(int));
    }

    for (it = model.movieIndex.begin(); it != model.movieIndex.end(); ++it)
    {
        out.write((char *)&it->first, sizeof(int));
        out.write((char *)&it->second, sizeof(int));
    }

    out.close();
    cout << "MF: model saved to " << filename << "\n";
}

MFModel loadMFModel(const string &filename)
{
    MFModel model;

    ifstream in(filename, ios::binary);
    if (!in.is_open())
    {
        return model;
    }

    int K;
    int numUsers;
    int numMovies;

    in.read((char *)&K, sizeof(int));
    in.read((char *)&numUsers, sizeof(int));
    in.read((char *)&numMovies, sizeof(int));
    in.read((char *)&model.globalMean, sizeof(double));

    if (K <= 0 || numUsers <= 0 || numMovies <= 0)
    {
        return model;
    }

    model.P.assign(numUsers, vector<double>(K));
    model.Q.assign(numMovies, vector<double>(K));
    model.userBias.assign(numUsers, 0.0);
    model.movieBias.assign(numMovies, 0.0);
    model.indexToUser.resize(numUsers);
    model.indexToMovie.resize(numMovies);

    for (int i = 0; i < (int)model.P.size(); i++)
    {
        in.read((char *)model.P[i].data(), K * sizeof(double));
    }

    for (int i = 0; i < (int)model.Q.size(); i++)
    {
        in.read((char *)model.Q[i].data(), K * sizeof(double));
    }

    in.read((char *)model.userBias.data(), numUsers * sizeof(double));
    in.read((char *)model.movieBias.data(), numMovies * sizeof(double));

    for (int i = 0; i < numUsers; i++)
    {
        int rawId;
        int idx;

        in.read((char *)&rawId, sizeof(int));
        in.read((char *)&idx, sizeof(int));

        model.userIndex[rawId] = idx;
        model.indexToUser[idx] = rawId;
    }

    for (int i = 0; i < numMovies; i++)
    {
        int rawId;
        int idx;

        in.read((char *)&rawId, sizeof(int));
        in.read((char *)&idx, sizeof(int));

        model.movieIndex[rawId] = idx;
        model.indexToMovie[idx] = rawId;
    }

    in.close();

    model.trained = true;
 //cout<< "MF: model loaded from " << filename << "\n";

    return model;
}

double mfPredict(int userId, int movieId, const MFModel &model)
{
    if (!model.trained)
    {
        return model.globalMean;
    }

    unordered_map<int, int>::const_iterator uit = model.userIndex.find(userId);
    unordered_map<int, int>::const_iterator mit = model.movieIndex.find(movieId);

    if (uit == model.userIndex.end() || mit == model.movieIndex.end())
    {
        return model.globalMean;
    }

    int u = uit->second;
    int m = mit->second;
    int K = (int)model.P[u].size();

    double pred = model.globalMean + model.userBias[u] + model.movieBias[m];

    for (int k = 0; k < K; k++)
    {
        pred += model.P[u][k] * model.Q[m][k];
    }

    return max(0.5, min(5.0, pred));
}

unordered_map<int, double> getMFScores(
    int userId,
    const MFModel &model,
    const unordered_set<int> &ratedMovies)
{
    unordered_map<int, double> mfScores;

    if (!model.trained)
    {
        return mfScores;
    }

    for (int m = 0; m < (int)model.indexToMovie.size(); m++)
    {
        int movieId = model.indexToMovie[m];

        if (ratedMovies.count(movieId))
        {
            continue;
        }

        double pred = mfPredict(userId, movieId, model);
        mfScores[movieId] = pred / 5.0;
    }

    return mfScores;
}

MFModel initMF(const string &ratingsFile)
{
    const string modelFile = "mf_model.bin";

    bool modelStale = false;

    {
        struct stat ratingStat;
        struct stat modelStat;

        if (stat(ratingsFile.c_str(), &ratingStat) == 0 &&
            stat(modelFile.c_str(), &modelStat) == 0)
        {
            if (ratingStat.st_mtime > modelStat.st_mtime)
            {
                cout << "MF: ratings file is newer than saved model; retraining...\n";
                modelStale = true;
            }
        }
    }

    if (!modelStale)
    {
        MFModel model = loadMFModel(modelFile);

        if (model.trained)
        {
            return model;
        }
    }

    cout << "MF: no saved model found - training now (one-time, ~30-60 seconds)...\n";

    MFModel model = trainMF(ratingsFile);

    if (model.trained)
    {
        saveMFModel(model, modelFile);
    }

    return model;
}
