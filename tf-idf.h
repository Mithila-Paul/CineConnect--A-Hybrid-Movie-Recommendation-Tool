#ifndef TF_IDF_H
#define TF_IDF_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace std;

using Vector = unordered_map<string, double>;

vector<string> tokenize(const string &text);

vector<pair<int, string>> readMoviesFromCSV(const string &filename);

unordered_map<string, double> computeTF(const vector<string> &words);
unordered_map<string, int> computeDF(const vector<vector<string>> &docs);
unordered_map<string, double> computeTFIDF(
    const unordered_map<string, double> &tf,
    const unordered_map<string, int> &df,
    int totalDocs);

void saveMovieVectorsToFile(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    const string &filename);

unordered_map<int, vector<string>> extractMovieTopics(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    int topWords = 15);

void saveTopicsToFile(
    const unordered_map<int, vector<string>> &topics);

void generateGenreFile(const string &csvFile, const string &outFile);

void initializeTFIDFCache();

const std::unordered_map<std::string, int> &getGlobalDF();

int getGlobalTotalDocs();

#endif
