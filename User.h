#ifndef USER_H
#define USER_H

#include <string>
#include <vector>
#include "trends.h"

void registerUser(const TrendsData &trends);
bool loginUser(const TrendsData &trends);

void userMenu(int userId, const TrendsData &trends);

void rateMovie(int userId);
void showMyRatings(int userId);
void recommendMoviesForUser(int userId, const TrendsData &trends);

bool hasRatedMovie(int userId, int movieId);
void appendToGlobalRatings(int userId, int movieId, double rating);
void removeFromGlobalRatings(int userId, int movieId);
void removeFromUserFile(int userId, int movieId);

void runColdStartOnboarding(int userId, const TrendsData &trends);

std::vector<std::string> chooseFavoriteGenres(
    int maxGenres = 3,
    const TrendsData *trends = nullptr);

std::string getGenreDisplayName(const std::string &genreKey);

#endif
