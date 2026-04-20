#ifndef GENRE_H
#define GENRE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// ── Data structures ───────────────────────────────────────────────────────────

struct MovieGenreInfo
{
    int    movieId;
    std::string primaryGenre;                        // genre with highest weight
    std::unordered_map<std::string, double> weights; // genre -> weight (sum = 1)
};

// Full genre data: movieId -> MovieGenreInfo
using GenreMap = std::unordered_map<int, MovieGenreInfo>;

// ── File I/O ──────────────────────────────────────────────────────────────────

// Load movie_genres.txt into memory.
// Format per line: movieId|primaryGenre|genre1:w1,genre2:w2,...
GenreMap loadGenreMap(const std::string &filename);

// ── Search helpers ────────────────────────────────────────────────────────────

// Returns movieIds whose primaryGenre matches (case-insensitive).
// Used for genre-search candidate pre-filtering.
std::vector<int> getMoviesByPrimaryGenre(
    const std::string &genre,
    const GenreMap &gmap);

// Returns movieIds that contain the genre with weight >= threshold.
// Includes cross-genre movies (e.g. an Action movie with 30% Romance still appears).
std::vector<int> getMoviesByGenreThreshold(
    const std::string &genre,
    const GenreMap &gmap,
    double threshold = 0.15);

// Returns movieIds whose tags contain the actor name (concatenated lowercase).
// e.g. "Leonardo DiCaprio" -> searches for "leonardodicaprio"
std::vector<int> getMoviesByActor(
    const std::string &actorName,
    const std::string &csvFile);

// Returns movieIds whose tags contain the director name (concatenated lowercase).
// Director is always the last space-separated token in the tags field.
std::vector<int> getMoviesByDirector(
    const std::string &directorName,
    const std::string &csvFile);

// Returns movieIds whose plot text (beginning of tags before genre cluster) 
// contains all the given keywords (case-insensitive substring match).
std::vector<int> getMoviesByPlot(
    const std::string &plotQuery,
    const std::string &csvFile);

// Normalise a name for tag matching: lowercase, remove spaces
// e.g. "Leonardo DiCaprio" -> "leonardodicaprio"
std::string normaliseName(const std::string &name);

// ── Genre weight score ────────────────────────────────────────────────────────

// Returns the weight of a specific genre for a movie (0.0 if not present).
// Used by the hybrid scorer to boost genre-relevant movies.
double getGenreWeight(int movieId, const std::string &genre, const GenreMap &gmap);

// ── Constants ─────────────────────────────────────────────────────────────────

// All valid genre names (lowercase, as they appear in movie_genres.txt)
const std::vector<std::string> ALL_GENRES = {
    "action", "adventure", "animation", "comedy", "crime",
    "documentary", "drama", "fantasy", "history", "horror",
    "music", "mystery", "romance", "sciencefiction", "thriller",
    "war", "western", "family", "sport"
};

// Display names for the genre list shown to users
const std::vector<std::pair<std::string,std::string>> GENRE_DISPLAY = {
    {"action",         "Action"},
    {"adventure",      "Adventure"},
    {"animation",      "Animation"},
    {"comedy",         "Comedy"},
    {"crime",          "Crime"},
    {"documentary",    "Documentary"},
    {"drama",          "Drama"},
    {"fantasy",        "Fantasy"},
    {"history",        "History"},
    {"horror",         "Horror"},
    {"music",          "Music"},
    {"mystery",        "Mystery"},
    {"romance",        "Romance"},
    {"sciencefiction", "Science Fiction"},
    {"thriller",       "Thriller"},
    {"war",            "War"},
    {"western",        "Western"},
    {"family",         "Family"},
    {"sport",          "Sport"}
};

#endif