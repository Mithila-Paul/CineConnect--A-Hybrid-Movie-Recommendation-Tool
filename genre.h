#ifndef GENRE_H
#define GENRE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct MovieGenreInfo
{
    int movieId;
    std::string primaryGenre;
    std::unordered_map<std::string, double> weights;
};

using GenreMap = std::unordered_map<int, MovieGenreInfo>;

GenreMap loadGenreMap(const std::string &filename);

std::vector<int> getMoviesByPrimaryGenre(
    const std::string &genre,
    const GenreMap &gmap);

std::vector<int> getMoviesByGenreThreshold(
    const std::string &genre,
    const GenreMap &gmap,
    double threshold = 0.15);

std::vector<int> getMoviesByActor(
    const std::string &actorName,
    const std::string &csvFile);

std::vector<int> getMoviesByDirector(
    const std::string &directorName,
    const std::string &csvFile);

std::vector<int> getMoviesByPlot(
    const std::string &plotQuery,
    const std::string &csvFile);

std::string normaliseName(const std::string &name);

double getGenreWeight(int movieId, const std::string &genre, const GenreMap &gmap);

extern const std::vector<std::string> ALL_GENRES;
extern const std::vector<std::pair<std::string, std::string>> GENRE_DISPLAY;

#endif
