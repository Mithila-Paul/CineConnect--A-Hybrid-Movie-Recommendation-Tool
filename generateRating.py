import pandas as pd
import re

# Load datasets
movies_synced = pd.read_csv('movies_synchronized.csv')
ml_movies = pd.read_csv('ml-latest-small/movies.csv')
ml_ratings = pd.read_csv('ml-latest-small/ratings.csv')

def clean_title(title):
    # Remove year (YYYY), lowercase, and remove special characters
    title = re.sub(r'\s*\(\d{4}\)', '', str(title))
    title = title.strip().lower()
    title = re.sub(r'[^a-z0-9]', '', title)
    return title

# Clean titles for matching
movies_synced['clean_title'] = movies_synced['title'].apply(clean_title)
ml_movies['clean_title'] = ml_movies['title'].apply(clean_title)

# Map synchronized ID to MovieLens ID
mapping = pd.merge(
    movies_synced[['movie_id', 'clean_title']], 
    ml_movies[['movieId', 'clean_title']], 
    on='clean_title'
)

# Create final ratings file
final_ratings = pd.merge(ml_ratings, mapping, on='movieId')
ratings_output = final_ratings[['movie_id', 'userId', 'rating']]
ratings_output.columns = ['movieId', 'userId', 'rating'] # Renaming for consistency

# Save to CSV
ratings_output.to_csv('ratings_processed.csv', index=False)