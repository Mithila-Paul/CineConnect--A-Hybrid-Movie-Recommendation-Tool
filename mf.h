#ifndef MF_H
#define MF_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

// ─── MF model ─────────────────────────────────────────────────────────────────
// Stores the trained latent factor matrices and bias terms.
// All indices are internal (0-based) — use userIndex/movieIndex maps to convert.
struct MFModel
{
    // P[u][k] = user u's affinity for latent factor k
    // Q[m][k] = movie m's expression of latent factor k
    std::vector<std::vector<double>> P; // [numUsers][K]
    std::vector<std::vector<double>> Q; // [numMovies][K]

    std::vector<double> userBias;       // bu: user's personal rating offset
    std::vector<double> movieBias;      // bm: movie's global quality offset
    double              globalMean;     // μ: mean of all ratings

    // Index maps: raw ID → row/col index in P / Q
    std::unordered_map<int, int> userIndex;
    std::unordered_map<int, int> movieIndex;

    // Reverse maps: index → raw ID (for output)
    std::vector<int> indexToUser;
    std::vector<int> indexToMovie;

    bool trained = false;
};

// ─── Training ─────────────────────────────────────────────────────────────────

// Train the MF model using Stochastic Gradient Descent.
// Reads ratings_processed.csv, builds P and Q matrices.
// Saves trained model to mf_model.bin for reuse across sessions.
//
// Parameters (with rationale):
//   K       = 20   latent factors — enough signal for 2701 movies, not overfit
//   epochs  = 20   SGD passes — tested convergence point for this dataset size
//   lr      = 0.005  learning rate — conservative, stable convergence
//   lambda  = 0.02   L2 regularization — prevents overfitting on sparse data
MFModel trainMF(
    const std::string &ratingsFile,
    int    K      = 20,
    int    epochs = 20,
    double lr     = 0.005,
    double lambda = 0.02);

// ─── Persistence ──────────────────────────────────────────────────────────────

// Save trained model to binary file — avoids retraining on every launch.
void saveMFModel(const MFModel &model, const std::string &filename);

// Load previously trained model. Returns model with trained=false if file missing.
MFModel loadMFModel(const std::string &filename);

// ─── Prediction ───────────────────────────────────────────────────────────────

// Predict the rating user would give movie.
// Formula: μ + bu + bm + P[u] · Q[m]
// Returns globalMean if user or movie not in training data.
double mfPredict(int userId, int movieId, const MFModel &model);

// Returns movieId -> predicted score (normalized to [0,1]) for all unrated movies.
// Used by the hybrid scorer for O(1) MF score lookup.
std::unordered_map<int, double> getMFScores(
    int userId,
    const MFModel &model,
    const std::unordered_set<int> &ratedMovies);

// ─── Initialization helper ────────────────────────────────────────────────────

// Called from initializeSystem(). Trains if no saved model exists.
// Returns loaded or freshly trained model.
MFModel initMF(const std::string &ratingsFile);

#endif
