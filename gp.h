#ifndef GP_H
#define GP_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

// =============================================================================
// gp.h  —  Graph Propagation for movie recommendation
//
// Graph model
// ───────────
// Nodes  : users  (id >= 1)  and  movies  (id stored as negative: -movieId
//          so both live in one integer namespace without collision)
// Edges  :
//   user u  → movie m   weight = normalised rating r(u,m) / max_rating
//   movie m → user u    weight = 1 / degree(m)   (uniform out-edge)
//
// The bipartite structure means information flows:
//   seed movies  →  users who liked them  →  movies those users liked  → …
// which is exactly "users who liked what you liked, also liked these."
//
// Graph Propagation
// ─────────────────
// score_new(v) = α · seed(v)  +  (1-α) · Σ_u [ score(u) · w(u→v) ]
//
//   α (teleport)  = 0.15  — standard value; lower → more propagation,
//                           higher → stays near seed set
//   iterations    = 3     — practical small-step propagation depth for this
//                           project
//   seed(v)       = normalised_rating(v)  for v ∈ user's seed movies,
//                   0 otherwise
//
// After propagation, only movie-node scores are returned, already
// normalised to [0,1] for direct use in the hybrid scorer.
// =============================================================================

// ratings[userId][movieId] = rating  (same type as RatingsMap in cf.h)
using GPRatingsMap = std::unordered_map<int, std::unordered_map<int, double>>;

// Build the bipartite graph from a ratings map and run GP seeded on the
// target user's rated movies.
//
// Returns: movieId -> GP score in [0,1] for every movie NOT in ratedMovies.
//
// Parameters:
//   ratingsMap   pre-loaded ratings (reuse the one from CF to avoid double I/O)
//   userId       target user
//   ratedMovies  movies already rated — excluded from output, used as seed set
//   alpha        teleport probability (default 0.15)
//   iterations   propagation steps (default 3)
std::unordered_map<int, double> computeGP(
    const GPRatingsMap           &ratingsMap,
    int                           userId,
    const std::unordered_set<int> &ratedMovies,
    double                        alpha      = 0.15,
    int                           iterations = 3);

#endif // GP_H