#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <string>
#include <algorithm>
#include <cctype>

using namespace std;

// for ignoring common stop words
unordered_set<string> stopWords = {
    "the","is","a","an","and","or","of","to","in","on","for","with",
    "by","at","from","as","that","this","it","was","are","be",
    "but","between"
};


vector<string> tokenize(const string &text) {
    stringstream ss(text);
    vector<string> words;
    string word;

    while (ss >> word) {

        
        transform(word.begin(), word.end(), word.begin(), ::tolower); // transforming to lower case

        word.erase(remove_if(word.begin(), word.end(), ::ispunct), word.end()); // it is for removing punctuation

        if (!word.empty() && stopWords.find(word) == stopWords.end()) {
            words.push_back(word); // only add if not a stop word
        }
    }
    return words;
}

// csv reader for movie_id and tags
vector<pair<int, string>> readMoviesFromCSV(const string &filename) {
    ifstream file(filename);
    vector<pair<int, string>> movies;

    if (!file.is_open()) {
        cout << "Error opening file\n";
        return movies;
    }

    string line;
    getline(file, line); // skip header

    while (getline(file, line)) {
        stringstream ss(line);
        string idStr, title, tags;

        getline(ss, idStr, ',');   // movie id
        getline(ss, title, ',');   // title which is ignored
        getline(ss, tags);         // tags

        int movieID = stoi(idStr);

        // quotation removal
        if (!tags.empty() && tags.front() == '"') tags.erase(0, 1);
        if (!tags.empty() && tags.back() == '"') tags.pop_back();

        movies.push_back({movieID, tags});
    }

    file.close();
    return movies;
}

//tf
unordered_map<string, double> computeTF(const vector<string> &words) {
    unordered_map<string, double> tf;

    for (const string &w : words)
        tf[w]++;

    double totalWords = words.size();
    for (auto &it : tf)
        it.second /= totalWords;

    return tf;
}

//df
unordered_map<string, int> computeDF(const vector<vector<string>> &docs) {
    unordered_map<string, int> df;

    for (const auto &doc : docs) {
        unordered_set<string> uniqueWords(doc.begin(), doc.end());
        for (const string &w : uniqueWords)
            df[w]++;
    }
    return df;
}

//tf-idf
unordered_map<string, double> computeTFIDF(
    const unordered_map<string, double> &tf,
    const unordered_map<string, int> &df,
    int totalDocs) {

    unordered_map<string, double> tfidf;

    for (auto &it : tf) {
        const string &word = it.first;
        double idf = log((double)totalDocs / df.at(word));
        tfidf[word] = it.second * idf;
    }

    return tfidf;
}

// save movie vectors to file
void saveMovieVectorsToFile(
    const vector<pair<int, unordered_map<string, double>>> &movieVectors,
    const string &filename
) {
    ofstream out(filename);

    for (auto &mv : movieVectors) {
        out << mv.first; // movie ID

        for (auto &it : mv.second) {
            out << " " << it.first << ":" << it.second;
        }
        out << "\n";
    }

    out.close();
}


int main() {
    string filename = "movies_processed.csv";

    // read movie id + tags
    vector<pair<int, string>> movies = readMoviesFromCSV(filename);
    int N = movies.size();

    // tokenization
    vector<vector<string>> tokenizedDocs;
    for (auto &m : movies)
        tokenizedDocs.push_back(tokenize(m.second));

    //df
    unordered_map<string, int> df = computeDF(tokenizedDocs);

    //tf-idf with real movie id
    vector<pair<int, unordered_map<string, double>>> movieVectors;

    for (int i = 0; i < N; i++) {
        auto tf = computeTF(tokenizedDocs[i]);
        auto tfidf = computeTFIDF(tf, df, N);
        movieVectors.push_back({movies[i].first, tfidf});
    }

    // save
    saveMovieVectorsToFile(movieVectors, "movie_vectors.txt");

    cout << "TF-IDF vectors saved successfully.\n";
    return 0;
}
