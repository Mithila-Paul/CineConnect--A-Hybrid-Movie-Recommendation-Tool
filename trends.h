#ifndef TRENDS_H
#define TRENDS_H

#include <string>
#include <vector>
#include <unordered_map>

struct GenreTrend
{
    std::string genre;
    std::string displayName;
    double avgRating;
    int ratingCount;
    double popularityIdx;
    double popularityPct;
};

struct MovieTrend
{
    int movieId;
    std::string title;
    std::string primaryGenre;
    double avgRating;
    double bayesianAvg;
    int ratingCount;
    std::unordered_map<std::string, double> genreWeights;
};

struct ActorTrend
{
    std::string token;
    double avgRating;
    int movieCount;
};

struct TrendsData
{
    std::vector<GenreTrend> genres;
    std::vector<MovieTrend> topMovies;
    std::vector<ActorTrend> topActors;

    double globalMean = 3.5;
    bool ready = false;
};

TrendsData computeTrends(
    const std::string &ratingsFile = "ratings_processed.csv",
    const std::string &moviesFile = "movies_synchronized.csv",
    const std::string &genresFile = "movie_genres.txt");

void showCurrentTrends(const TrendsData &trends);

std::vector<int> getTrendingMoviesByGenre(
    const std::string &genre,
    const TrendsData &trends,
    int topN = 5,
    double threshold = 0.15);

std::vector<std::string> getTopTrendingGenres(
    const TrendsData &trends,
    int topN = 3);

#endif