#ifndef TF_IDF_H
#define TF_IDF_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace std;

// Type alias for sparse TF-IDF vectors
using Vector = unordered_map<string, double>;

// Tokenization / preprocessing
vector<string> tokenize(const string &text);

// CSV reading
vector<pair<int, string>> readMoviesFromCSV(const string &filename);

// TF / DF / TF-IDF
unordered_map<string, double> computeTF(const vector<string> &words);
unordered_map<string, int> computeDF(const vector<vector<string>> &docs);
unordered_map<string, double> computeTFIDF(
    const unordered_map<string, double> &tf,
    const unordered_map<string, int> &df,
    int totalDocs);

// Save generated movie vectors
void saveMovieVectorsToFile(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    const string &filename);

// Topic extraction and saving
unordered_map<int, vector<string>> extractMovieTopics(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    int topWords = 5);

void saveTopicsToFile(
    const unordered_map<int, vector<string>> &topics);

// Genre file generation
void generateGenreFile(const string &csvFile, const string &outFile);

// System initialization
void initializeSystem();

#endif
