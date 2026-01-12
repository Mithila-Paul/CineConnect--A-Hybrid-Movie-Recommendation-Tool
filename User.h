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

#endif
