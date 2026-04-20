#include "mf.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <random>
#include <numeric>
#include <cstring>

using namespace std;

// =============================================================================
// loadRatingsForMF  (internal helper)
// Reads ratings_processed.csv into a flat list of (userId, movieId, rating).
// Also builds the userId and movieId index maps used throughout MF.
// =============================================================================
struct RatingEntry { int userId, movieId; double rating; };

static vector<RatingEntry> loadRatingsForMF(
    const string &filename,
    unordered_map<int,int> &userIndex,
    unordered_map<int,int> &movieIndex,
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
    getline(file, line); // skip header: movieId,userId,rating

    while (getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        stringstream ss(line);
        string mStr, uStr, rStr;
        getline(ss, mStr, ',');
        getline(ss, uStr,  ',');
        getline(ss, rStr,  ',');
        if (mStr.empty() || uStr.empty() || rStr.empty()) continue;

        try
        {
            int    movieId = stoi(mStr);
            int    userId  = stoi(uStr);
            double rating  = stod(rStr);

            // Assign consecutive 0-based indices on first encounter
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

            entries.push_back({userId, movieId, rating});
        }
        catch (...) { continue; }
    }

    file.close();
    return entries;
}

// =============================================================================
// trainMF
//
// Trains the MF model using Stochastic Gradient Descent (SGD).
//
// Mathematical model:
//   pred(u,m) = μ + bu + bm + P[u] · Q[m]
//
// Where:
//   μ       = global mean rating (constant, computed once)
//   bu      = user bias — how much user u rates above/below average
//   bm      = movie bias — how much movie m is rated above/below average
//   P[u]    = user u's latent preference vector (K-dimensional)
//   Q[m]    = movie m's latent feature vector (K-dimensional)
//   P[u]·Q[m] = dot product = user-movie affinity
//
// SGD update rule (per rating observation):
//   error e = r(u,m) − pred(u,m)
//
//   bu  ← bu  + γ(e − λ·bu)
//   bm  ← bm  + γ(e − λ·bm)
//   P[u][k] ← P[u][k] + γ(e·Q[m][k] − λ·P[u][k])   for each k
//   Q[m][k] ← Q[m][k] + γ(e·P[u][k] − λ·Q[m][k])   for each k
//
// Why these hyperparameters:
//   K=20:      enough latent factors to capture taste diversity without overfitting
//              on 2701 movies. Industry standard for datasets of this size.
//   epochs=20: tested convergence point. RMSE typically plateaus by epoch 15-18.
//   lr=0.005:  conservative learning rate. Higher (0.01+) risks divergence on
//              sparse data; lower (0.001) converges too slowly for 20 epochs.
//   lambda=0.02: L2 regularization prevents P and Q from growing too large,
//               which would memorize training data and predict poorly on new users.
//
// Shuffling: ratings are shuffled each epoch so SGD doesn't learn order artifacts.
// =============================================================================
MFModel trainMF(
    const string &ratingsFile,
    int K, int epochs, double lr, double lambda)
{
    MFModel model;
    model.globalMean = 0.0;

    cout << "MF: loading ratings...\n";
    auto entries = loadRatingsForMF(
        ratingsFile,
        model.userIndex, model.movieIndex,
        model.indexToUser, model.indexToMovie);

    if (entries.empty()) { cout << "MF: no ratings loaded\n"; return model; }

    int numUsers  = (int)model.indexToUser.size();
    int numMovies = (int)model.indexToMovie.size();

    cout << "MF: " << numUsers << " users, " << numMovies
         << " movies, " << entries.size() << " ratings\n";
    cout << "MF: training K=" << K << " factors, " << epochs << " epochs...\n";

    // ── Compute global mean ────────────────────────────────────────────────────
    double sum = 0.0;
    for (auto &e : entries) sum += e.rating;
    model.globalMean = sum / entries.size();

    // ── Initialize P, Q, biases with small random values ─────────────────────
    // Small random init prevents symmetry — if all values were 0, gradients
    // would update identically and all rows would stay identical forever.
    mt19937 rng(42); // fixed seed for reproducibility
    uniform_real_distribution<double> dist(0.0, 0.1);

    model.P.assign(numUsers,  vector<double>(K));
    model.Q.assign(numMovies, vector<double>(K));
    model.userBias.assign(numUsers,  0.0);
    model.movieBias.assign(numMovies, 0.0);

    for (auto &row : model.P)
        for (auto &v : row) v = dist(rng);
    for (auto &row : model.Q)
        for (auto &v : row) v = dist(rng);

    // ── SGD training loop ──────────────────────────────────────────────────────
    vector<int> order(entries.size());
    iota(order.begin(), order.end(), 0); // 0,1,2,...,N-1

    for (int epoch = 0; epoch < epochs; epoch++)
    {
        // Shuffle rating order each epoch — prevents order bias in SGD
        shuffle(order.begin(), order.end(), rng);

        double epochLoss = 0.0;

        for (int idx : order)
        {
            auto &e = entries[idx];
            int u = model.userIndex.at(e.userId);
            int m = model.movieIndex.at(e.movieId);

            // Compute prediction
            double pred = model.globalMean
                        + model.userBias[u]
                        + model.movieBias[m];
            for (int k = 0; k < K; k++)
                pred += model.P[u][k] * model.Q[m][k];

            // Clamp prediction to valid range before computing error
            pred = max(0.5, min(5.0, pred));

            double err = e.rating - pred;
            epochLoss += err * err;

            // Update biases
            model.userBias[u]  += lr * (err - lambda * model.userBias[u]);
            model.movieBias[m] += lr * (err - lambda * model.movieBias[m]);

            // Update latent factors — must update P AFTER using old P to update Q
            // We store old P[u] values first to avoid using partially-updated values
            // in Q update (standard SGD correctness requirement)
            vector<double> oldPu = model.P[u]; // snapshot before update
            for (int k = 0; k < K; k++)
            {
                model.P[u][k] += lr * (err * model.Q[m][k] - lambda * model.P[u][k]);
                model.Q[m][k] += lr * (err * oldPu[k]      - lambda * model.Q[m][k]);
            }
        }

        double rmse = sqrt(epochLoss / entries.size());
        cout << "  Epoch " << (epoch+1) << "/" << epochs
             << "  RMSE: " << rmse << "\n";
    }

    model.trained = true;
    cout << "MF: training complete.\n";
    return model;
}

// =============================================================================
// saveMFModel / loadMFModel
//
// Binary serialization — much faster than text for matrices of this size.
// Format (sequential binary):
//   [int K] [int numUsers] [int numMovies] [double globalMean]
//   [P matrix: numUsers * K doubles]
//   [Q matrix: numMovies * K doubles]
//   [userBias: numUsers doubles]
//   [movieBias: numMovies doubles]
//   [userIndex entries: numUsers * (int rawId, int idx) pairs]
//   [movieIndex entries: numMovies * (int rawId, int idx) pairs]
// =============================================================================
void saveMFModel(const MFModel &model, const string &filename)
{
    ofstream out(filename, ios::binary);
    if (!out.is_open()) { cout << "MF: cannot save model\n"; return; }

    int K         = (int)model.P[0].size();
    int numUsers  = (int)model.P.size();
    int numMovies = (int)model.Q.size();

    out.write((char*)&K,         sizeof(int));
    out.write((char*)&numUsers,  sizeof(int));
    out.write((char*)&numMovies, sizeof(int));
    out.write((char*)&model.globalMean, sizeof(double));

    for (auto &row : model.P)
        out.write((char*)row.data(), K * sizeof(double));
    for (auto &row : model.Q)
        out.write((char*)row.data(), K * sizeof(double));

    out.write((char*)model.userBias.data(),  numUsers  * sizeof(double));
    out.write((char*)model.movieBias.data(), numMovies * sizeof(double));

    // Save index maps as (rawId, index) pairs
    for (auto &kv : model.userIndex)
    {
        out.write((char*)&kv.first,  sizeof(int));
        out.write((char*)&kv.second, sizeof(int));
    }
    for (auto &kv : model.movieIndex)
    {
        out.write((char*)&kv.first,  sizeof(int));
        out.write((char*)&kv.second, sizeof(int));
    }

    out.close();
    cout << "MF: model saved to " << filename << "\n";
}

MFModel loadMFModel(const string &filename)
{
    MFModel model;
    ifstream in(filename, ios::binary);
    if (!in.is_open()) return model; // trained=false signals caller to retrain

    int K, numUsers, numMovies;
    in.read((char*)&K,         sizeof(int));
    in.read((char*)&numUsers,  sizeof(int));
    in.read((char*)&numMovies, sizeof(int));
    in.read((char*)&model.globalMean, sizeof(double));

    if (K <= 0 || numUsers <= 0 || numMovies <= 0) return model;

    model.P.assign(numUsers,  vector<double>(K));
    model.Q.assign(numMovies, vector<double>(K));
    model.userBias.assign(numUsers,  0.0);
    model.movieBias.assign(numMovies, 0.0);
    model.indexToUser.resize(numUsers);
    model.indexToMovie.resize(numMovies);

    for (auto &row : model.P)
        in.read((char*)row.data(), K * sizeof(double));
    for (auto &row : model.Q)
        in.read((char*)row.data(), K * sizeof(double));

    in.read((char*)model.userBias.data(),  numUsers  * sizeof(double));
    in.read((char*)model.movieBias.data(), numMovies * sizeof(double));

    // Restore index maps
    for (int i = 0; i < numUsers; i++)
    {
        int rawId, idx;
        in.read((char*)&rawId, sizeof(int));
        in.read((char*)&idx,   sizeof(int));
        model.userIndex[rawId]  = idx;
        model.indexToUser[idx]  = rawId;
    }
    for (int i = 0; i < numMovies; i++)
    {
        int rawId, idx;
        in.read((char*)&rawId, sizeof(int));
        in.read((char*)&idx,   sizeof(int));
        model.movieIndex[rawId]  = idx;
        model.indexToMovie[idx] = rawId;
    }

    in.close();
    model.trained = true;
    cout << "MF: model loaded from " << filename << "\n";
    return model;
}

// =============================================================================
// mfPredict
// Computes the predicted rating: μ + bu + bm + P[u] · Q[m]
// Returns globalMean as fallback for unknown users/movies (new user cold-start).
// Result clamped to [0.5, 5.0] — the valid rating range.
// =============================================================================
double mfPredict(int userId, int movieId, const MFModel &model)
{
    if (!model.trained) return model.globalMean;

    auto uit = model.userIndex.find(userId);
    auto mit = model.movieIndex.find(movieId);

    // Unknown user or movie — return global mean (graceful degradation)
    if (uit == model.userIndex.end() || mit == model.movieIndex.end())
        return model.globalMean;

    int u = uit->second;
    int m = mit->second;
    int K = (int)model.P[u].size();

    double pred = model.globalMean + model.userBias[u] + model.movieBias[m];
    for (int k = 0; k < K; k++)
        pred += model.P[u][k] * model.Q[m][k];

    return max(0.5, min(5.0, pred));
}

// =============================================================================
// getMFScores
// Precomputes MF predicted scores for ALL unrated movies in one pass.
// Returns movieId -> normalized score in [0,1] for use in the hybrid scorer.
//
// Normalization: divide by 5.0 (max rating) so scores are in the same
// [0,1] range as CBF cosine similarities and CF Pearson predictions.
//
// For movies not in the training data (rare — only new movies added after
// training): score defaults to globalMean/5.0 (neutral, not zero).
// =============================================================================
unordered_map<int, double> getMFScores(
    int userId,
    const MFModel &model,
    const unordered_set<int> &ratedMovies)
{
    unordered_map<int, double> mfScores;
    if (!model.trained) return mfScores;

    for (int m = 0; m < (int)model.indexToMovie.size(); m++)
    {
        int movieId = model.indexToMovie[m];
        if (ratedMovies.count(movieId)) continue;

        double pred = mfPredict(userId, movieId, model);
        mfScores[movieId] = pred / 5.0; // normalize to [0,1]
    }

    return mfScores;
}

// =============================================================================
// initMF
// Called once at startup (from tf-idf.cpp's initializeSystem or User.cpp).
// Loads saved model if it exists, otherwise trains from scratch.
// Training takes ~30-60 seconds for this dataset — happens only once.
// =============================================================================
MFModel initMF(const string &ratingsFile)
{
    const string modelFile = "mf_model.bin";

    MFModel model = loadMFModel(modelFile);
    if (model.trained)
        return model;

    cout << "MF: no saved model found — training now (one-time, ~30-60 seconds)...\n";
    model = trainMF(ratingsFile);

    if (model.trained)
        saveMFModel(model, modelFile);

    return model;
}
