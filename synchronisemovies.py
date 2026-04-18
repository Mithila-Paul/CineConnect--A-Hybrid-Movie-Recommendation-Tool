import pandas as pd

# Load your processed movies
movies = pd.read_csv('movies_processed.csv')

# Create a new sequential ID starting from 1
movies['new_movie_id'] = range(1, len(movies) + 1)

# Save the mapping for future reference (important for the ratings file!)
movie_mapping = movies[['movie_id', 'new_movie_id', 'title']]
movies = movies[['new_movie_id', 'title', 'tags']] # Update the main file
movies.to_csv('movies_synchronized.csv', index=False)