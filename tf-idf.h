#ifndef TFIDF_H
#define TFIDF_H

#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

vector<string> tokenize(const string &text);

vector<string> readTagsFromCSV(const string &filename);

unordered_map<string, double> computeTF(const vector<string> &words);

unordered_map<string, int> computeDF(const vector<vector<string>> &docs);

unordered_map<string, double> computeTFIDF(
    const unordered_map<string, double> &tf,
    const unordered_map<string, int> &df,
    int totalDocs
);

#endif
