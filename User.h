#ifndef USER_H
#define USER_H

#include <string>

void registerUser();
bool loginUser();
void userMenu(int userId);

//menu 
void rateMovie(int userId);
void showMyRatings(int userId);
void recommendMoviesForUser(int userId);

// rating helpers (used internally, exposed for CF/MF integration later)
bool hasRatedMovie(int userId, int movieId);
void appendToGlobalRatings(int userId, int movieId, double rating);
void removeFromGlobalRatings(int userId, int movieId);
void removeFromUserFile(int userId, int movieId);

// cold-start onboarding helpers
void runColdStartOnboarding(int userId);
std::vector<std::string> chooseFavoriteGenres(int maxGenres = 3);
std::vector<int> getTopSeedMoviesByGenre(const std::string &genre, int count = 5);
std::string getGenreDisplayName(const std::string &genreKey);


#endif
