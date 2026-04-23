#include "system_init.h"
#include "tf-idf.h"
#include "mf.h"
#include <fstream>
#include <iostream>

using namespace std;

void initializeSystem()
{
    cout << "Checking system files...\n";

    ifstream vectorCheck("movie_vectors.txt");

    if (vectorCheck.is_open())
    {
        vectorCheck.close();

        ifstream genreCheck("movie_genres.txt");

        if (!genreCheck.is_open())
        {
            generateGenreFile("movies_synchronized.csv", "movie_genres.txt");
        }
        else
        {
            genreCheck.close();
        }

        initializeTFIDFCache();
        initMF("ratings_processed.csv");

        cout << "System data already initialized. Ready!\n";
        return;
    }

    cout << "Initializing system data... this may take a moment.\n";

    string filename = "movies_synchronized.csv";
    vector<pair<int, string>> movies = readMoviesFromCSV(filename);
    int N = (int)movies.size();

    vector<vector<string>> tokenizedDocs;
    tokenizedDocs.reserve(N);

    for (int i = 0; i < N; i++)
    {
        tokenizedDocs.push_back(tokenize(movies[i].second));
    }

    unordered_map<string, int> df = computeDF(tokenizedDocs);

    vector<pair<int, unordered_map<string, double>>> movieVectors;

    for (int i = 0; i < N; i++)
    {
        unordered_map<string, double> tf = computeTF(tokenizedDocs[i]);
        unordered_map<string, double> tfidf = computeTFIDF(tf, df, N);

        movieVectors.push_back(make_pair(movies[i].first, tfidf));
    }

    saveMovieVectorsToFile(movieVectors, "movie_vectors.txt");

    unordered_map<int, vector<string>> topics = extractMovieTopics(movieVectors, 15);
    saveTopicsToFile(topics);

    generateGenreFile("movies_synchronized.csv", "movie_genres.txt");

    initializeTFIDFCache();
    initMF("ratings_processed.csv");

    cout << "System initialized! Data files created successfully.\n";
}