# CineConnect--A-Hybrid-Movie-Recommendation-Tool
CineConnect is a smart movie recommendation system that combines multiple recommendation techniques to suggest movies tailored to each user’s taste. Instead of relying on a single algorithm, it blends different approaches to deliver more accurate, diverse, and personalized recommendations.

What Makes This Project Special?:
Most basic recommendation systems use only one method. CineConnect is different.
It uses a hybrid approach, combining:
Content-Based Filtering (CBF)
Collaborative Filtering (CF)
Matrix Factorization (MF)
Graph Propagation (GP)
Each method solves a different problem—and together, they create a much stronger system.
Trending / Statistical Ranking

How It Works (Simple Explanation):
When a user asks for recommendations, the system:
Understands the user’s taste from past ratings
Reads their current mood (search/query)
Finds similar users and patterns
Learns hidden preferences using machine learning
Spreads influence through a user-movie network
Combines everything into one final score
Then it shows the top recommended movies.

Algorithms Used:
1. Content-Based Filtering (CBF)
Uses movie features (genres, plot, keywords)
Represents movies using TF-IDF vectors
Uses cosine similarity to match user preferences

##Good for:
New users
Personalized recommendations


2.  Collaborative Filtering (CF)
Finds users with similar tastes
Uses Pearson similarity
Predicts ratings based on neighbors

#Good for:
Community-driven recommendations


3.  Matrix Factorization (MF)
Breaks data into hidden patterns (latent features)
Trained using Stochastic Gradient Descent (SGD)

#Good for:
Learning deep user preferences
Handling sparse data


4. Graph Propagation (GP)
Treats users and movies as a graph
Uses random walk / propagation
Spreads influence across connections

#Good for:
Discovering indirect relationships

Hybrid Recommendation Strategy
Instead of choosing one algorithm, CineConnect combines all of them.
Each movie gets a score like:
#Final Score = CBF + Query + Topic + CF + MF + GP + Genre

Weights are adaptive:
New users → more CBF
Experienced users → more CF, MF, GP

Cold Start Problem (Solved):
New users usually don’t have data. CineConnect handles this using:
Genre selection
Seed movie ratings
Query-based recommendations
Trending/popular movies fallback

Search Features:
Users can search in multiple ways:
1.By Genre
2. By Actor
3.By Director
4. By Plot / Keywords
5. General “craving” (free text)

Trending and Statistical Ranking:
This is the part that was missing before, and it is an important piece of the project.
The trending module is used to understand which movies are strong overall choices based on dataset-wide statistics, not only personal taste.
It uses ideas like:
average rating
rating count
weighted average
Bayesian average
logarithmic popularity scaling

Dataset:
~4800 movies, ~53,000 ratings,  610+ users

 Project Structure (Simplified):
cbf.cpp        → Content-based filtering
cf.cpp         → Collaborative filtering
mf.cpp         → Matrix factorization
gp.cpp         → Graph propagation
tf-idf.cpp     → Text processing
trends.cpp     → Popularity & statistics
User.cpp       → Main system + integration
system_init    → Initialization logic

User Flow:
Register → get user ID
Cold-start onboarding (choose genres + rate movies)
Login
Rate movies
Get recommendations
Search based on mood

Key Features:
Hybrid recommendation system
Personalized suggestions
Cold-start handling
Multi-mode search
Real-time scoring
Scalable architecture
Optimized performance (caching, normalization)
Trending/statistical usefulness in fallback or ranking suppor


CineConnect is not just a simple recommender—it’s a complete hybrid system that mimics how real-world platforms (like Netflix) combine multiple techniques to deliver better recommendations.
