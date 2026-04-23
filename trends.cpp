#include "trends.h"
#include "genre.h"
#include "csv_parser.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <unordered_set>
#include <unordered_map>
#include <cctype>

using namespace std;

struct MovieRow
{
    string title;
    string tags;
};

static unordered_map<int, MovieRow> parseMoviesCSV(const string &filename)
{
    unordered_map<int, MovieRow> result;

    ifstream f(filename);
    if (!f.is_open())
    {
        cout << "Trends: cannot open " << filename << "\n";
        return result;
    }

    string line;
    getline(f, line); 

    while (getline(f, line))
    {
        stripCR(line);

        if (line.empty())
        {
            continue;
        }

        istringstream ss(line);

        string idStr = parseCsvField(ss);
        string title = parseCsvField(ss);
        string tags;
        getline(ss, tags);

        if (!tags.empty() && tags[0] == '"')
        {
            tags.erase(0, 1);
        }

        if (!tags.empty() && tags[tags.size() - 1] == '"')
        {
            tags.erase(tags.size() - 1);
        }

        int movieId = -1;

        try
        {
            movieId = stoi(idStr);
        }
        catch (...)
        {
            continue;
        }

        MovieRow row;
        row.title = title;
        row.tags = tags;

        result[movieId] = row;
    }

    f.close();
    return result;
}

static string cleanToken(string tok)
{
    tok.erase(
        remove_if(
            tok.begin(),
            tok.end(),
            [](unsigned char c)
            {
                return ispunct(c);
            }),
        tok.end());

    transform(
        tok.begin(),
        tok.end(),
        tok.begin(),
        [](unsigned char c)
        {
            return (char)tolower(c);
        });

    return tok;
}

static vector<string> tokenizeTags(const string &tags)
{
    stringstream ss(tags);
    vector<string> tokens;
    string tok;

    while (ss >> tok)
    {
        tok = cleanToken(tok);

        if (!tok.empty())
        {
            tokens.push_back(tok);
        }
    }

    return tokens;
}

static unordered_map<int, vector<string> > extractActorsPerMovie(const unordered_map<int, MovieRow> &movies)
{
    unordered_map<int, vector<string> > movieActors;

    unordered_map<int, MovieRow>::const_iterator it;
    for (it = movies.begin(); it != movies.end(); ++it)
    {
        int movieId = it->first;
        vector<string> tokens = tokenizeTags(it->second.tags);

        if ((int)tokens.size() < 4)
        {
            continue;
        }

        int directorIndex = (int)tokens.size() - 1;
        int actorStart = max(0, directorIndex - 3);

        unordered_set<string> seen;

        for (int i = actorStart; i < directorIndex; i++)
        {
            string tok = tokens[i];

            if (tok.empty())
            {
                continue;
            }

            if (seen.count(tok))
            {
                continue;
            }

            seen.insert(tok);
            movieActors[movieId].push_back(tok);
        }
    }

    return movieActors;
}

static unordered_map<string, string> buildActorDisplayMap()
{
    unordered_map<string, string> m;

    m["gregorypeck"] = "Gregory Peck";
    m["pauldano"] = "Paul Dano";
    m["catherinedeneuve"] = "Catherine Deneuve";
    m["leonardodicaprio"] = "Leonardo DiCaprio";
    m["robertdowneyjr"] = "Robert Downey Jr";
    m["chrisevans"] = "Chris Evans";
    m["christianbale"] = "Christian Bale";
    m["tomhanks"] = "Tom Hanks";
    m["morganfreeman"] = "Morgan Freeman";
    m["bradpitt"] = "Brad Pitt";
    m["johntravolta"] = "John Travolta";
    m["samueljackson"] = "Samuel L. Jackson";
    m["alpacino"] = "Al Pacino";
    m["robertdeniro"] = "Robert De Niro";
    m["marlonbrando"] = "Marlon Brando";
    m["jacknicholson"] = "Jack Nicholson";
    m["russellcrowe"] = "Russell Crowe";
    m["joaquinphoenix"] = "Joaquin Phoenix";
    m["keanureeves"] = "Keanu Reeves";
    m["natalieportman"] = "Natalie Portman";
    m["scarlettjohansson"] = "Scarlett Johansson";
    m["mattdamon"] = "Matt Damon";
    m["benaffleck"] = "Ben Affleck";
    m["juliaroberts"] = "Julia Roberts";
    m["nicolekidman"] = "Nicole Kidman";
    m["katewinslet"] = "Kate Winslet";
    m["hughjackman"] = "Hugh Jackman";
    m["angelinajolie"] = "Angelina Jolie";
    m["willsmith"] = "Will Smith";
    m["jonnymdepp"] = "Johnny Depp";
    m["johnnydepp"] = "Johnny Depp";
    m["heathledger"] = "Heath Ledger";
    m["garyoldman"] = "Gary Oldman";
    m["christopherplummer"] = "Christopher Plummer";
    m["uma thurman"] = "Uma Thurman";
    m["umathurman"] = "Uma Thurman";
    m["harrydeanstanton"] = "Harry Dean Stanton";
    m["edwardnorton"] = "Edward Norton";
    m["helenabonhamcarter"] = "Helena Bonham Carter";
    m["michaelcaine"] = "Michael Caine";
    m["annehathaway"] = "Anne Hathaway";
    m["merylstreep"] = "Meryl Streep";
    m["dustinhoffman"] = "Dustin Hoffman";
    m["liamneeson"] = "Liam Neeson";
    m["ralphfiennes"] = "Ralph Fiennes";
    m["adrienbrody"] = "Adrien Brody";
    m["jodiefoster"] = "Jodie Foster";
    m["anthonyhopkins"] = "Anthony Hopkins";

    return m;
}

static string titleCaseWord(const string &w)
{
    if (w.empty())
    {
        return w;
    }

    string out = w;
    out[0] = (char)toupper((unsigned char)out[0]);

    for (int i = 1; i < (int)out.size(); i++)
    {
        out[i] = (char)tolower((unsigned char)out[i]);
    }

    return out;
}

static string fallbackActorDisplayName(const string &token)
{
    return titleCaseWord(token);
}

static string actorDisplayName(const string &token)
{
    static const unordered_map<string, string> DISPLAY_MAP = buildActorDisplayMap();

    unordered_map<string, string>::const_iterator it = DISPLAY_MAP.find(token);
    if (it != DISPLAY_MAP.end())
    {
        return it->second;
    }

    return fallbackActorDisplayName(token);
}

static bool compareMovies(const MovieTrend &a, const MovieTrend &b)
{
    if (fabs(a.bayesianAvg - b.bayesianAvg) > 1e-9)
    {
        return a.bayesianAvg > b.bayesianAvg;
    }

    return a.ratingCount > b.ratingCount;
}

static bool compareGenres(const GenreTrend &a, const GenreTrend &b)
{
    return a.popularityIdx > b.popularityIdx;
}

static bool compareActors(const ActorTrend &a, const ActorTrend &b)
{
    if (fabs(a.avgRating - b.avgRating) > 1e-4)
    {
        return a.avgRating > b.avgRating;
    }

    return a.movieCount > b.movieCount;
}

TrendsData computeTrends(
    const string &ratingsFile,
    const string &moviesFile,
    const string &genresFile)
{
    TrendsData data;

    GenreMap gmap = loadGenreMap(genresFile);
    unordered_map<int, MovieRow> movies = parseMoviesCSV(moviesFile);

    if (movies.empty())
    {
        cout << "Trends: no movie data loaded - aborting.\n";
        return data;
    }

    unordered_map<int, vector<string> > movieActors = extractActorsPerMovie(movies);

    unordered_map<int, double> movieRatingSum;
    unordered_map<int, int> movieRatingCount;

    double globalSum = 0.0;
    int globalCount = 0;

    ifstream f(ratingsFile);
    if (!f.is_open())
    {
        cout << "Trends: cannot open " << ratingsFile << "\n";
        return data;
    }

    string line;
    getline(f, line); // skip header

    while (getline(f, line))
    {
        stripCR(line);

        if (line.empty())
        {
            continue;
        }

        istringstream ss(line);
        string mStr = parseCsvField(ss);
        string uStr = parseCsvField(ss);
        string rStr = parseCsvField(ss);

        if (mStr.empty() || rStr.empty())
        {
            continue;
        }

        try
        {
            int movieId = stoi(mStr);
            double rating = stod(rStr);

            movieRatingSum[movieId] += rating;
            movieRatingCount[movieId] += 1;
            globalSum += rating;
            globalCount += 1;
        }
        catch (...)
        {
        }
    }

    f.close();

    if (globalCount > 0)
    {
        data.globalMean = globalSum / globalCount;
    }
    else
    {
        data.globalMean = 3.5;
    }

    double C = (double)globalCount / max(1, (int)movieRatingCount.size());

    unordered_map<int, int>::const_iterator mcIt;
    for (mcIt = movieRatingCount.begin(); mcIt != movieRatingCount.end(); ++mcIt)
    {
        int movieId = mcIt->first;
        int count = mcIt->second;
        double avg = movieRatingSum[movieId] / count;
        double bayes = (count * avg + C * data.globalMean) / (count + C);

        unordered_map<int, MovieRow>::const_iterator mit = movies.find(movieId);
        if (mit == movies.end())
        {
            continue;
        }

        string primaryGenre;
        unordered_map<string, double> weights;

        GenreMap::const_iterator git = gmap.find(movieId);
        if (git != gmap.end())
        {
            primaryGenre = git->second.primaryGenre;
            weights = git->second.weights;
        }

        MovieTrend mt;
        mt.movieId = movieId;
        mt.title = mit->second.title;
        mt.primaryGenre = primaryGenre;
        mt.avgRating = avg;
        mt.bayesianAvg = bayes;
        mt.ratingCount = count;
        mt.genreWeights = weights;

        data.topMovies.push_back(mt);
    }

    sort(data.topMovies.begin(), data.topMovies.end(), compareMovies);

    unordered_map<string, double> genreWeightedSum;
    unordered_map<string, double> genreEffectiveCount;

    for (mcIt = movieRatingCount.begin(); mcIt != movieRatingCount.end(); ++mcIt)
    {
        int movieId = mcIt->first;
        int count = mcIt->second;
        double avg = movieRatingSum[movieId] / count;

        GenreMap::const_iterator git = gmap.find(movieId);
        if (git == gmap.end())
        {
            continue;
        }

        unordered_map<string, double>::const_iterator gw;
        for (gw = git->second.weights.begin(); gw != git->second.weights.end(); ++gw)
        {
            string genre = gw->first;
            double w = gw->second;

            genreWeightedSum[genre] += avg * w * count;
            genreEffectiveCount[genre] += w * count;
        }
    }

    double totalPopularity = 0.0;
    vector<GenreTrend> rawGenres;

    unordered_map<string, double>::const_iterator geIt;
    for (geIt = genreEffectiveCount.begin(); geIt != genreEffectiveCount.end(); ++geIt)
    {
        string gkey = geIt->first;
        double effCount = geIt->second;

        if (effCount < 1.0)
        {
            continue;
        }

        double avg = genreWeightedSum[gkey] / effCount;
        double popIdx = avg * log(1.0 + effCount);

        string displayName = gkey;

        for (int i = 0; i < (int)GENRE_DISPLAY.size(); i++)
        {
            if (GENRE_DISPLAY[i].first == gkey)
            {
                displayName = GENRE_DISPLAY[i].second;
                break;
            }
        }

        totalPopularity += popIdx;

        GenreTrend gt;
        gt.genre = gkey;
        gt.displayName = displayName;
        gt.avgRating = avg;
        gt.ratingCount = (int)effCount;
        gt.popularityIdx = popIdx;
        gt.popularityPct = 0.0;

        rawGenres.push_back(gt);
    }

    for (int i = 0; i < (int)rawGenres.size(); i++)
    {
        if (totalPopularity > 0.0)
        {
            rawGenres[i].popularityPct =
                (rawGenres[i].popularityIdx / totalPopularity) * 100.0;
        }
        else
        {
            rawGenres[i].popularityPct = 0.0;
        }
    }

    sort(rawGenres.begin(), rawGenres.end(), compareGenres);
    data.genres = rawGenres;

    unordered_map<string, double> actorRatingSum;
    unordered_map<string, int> actorMovieCount;

    unordered_map<int, vector<string> >::const_iterator maIt;
    for (maIt = movieActors.begin(); maIt != movieActors.end(); ++maIt)
    {
        int movieId = maIt->first;

        unordered_map<int, int>::const_iterator cntIt = movieRatingCount.find(movieId);
        if (cntIt == movieRatingCount.end())
        {
            continue;
        }

        double avg = movieRatingSum[movieId] / cntIt->second;

        for (int i = 0; i < (int)maIt->second.size(); i++)
        {
            string actorTok = maIt->second[i];
            actorRatingSum[actorTok] += avg;
            actorMovieCount[actorTok] += 1;
        }
    }

    unordered_map<string, int>::const_iterator acIt;
    for (acIt = actorMovieCount.begin(); acIt != actorMovieCount.end(); ++acIt)
    {
        string token = acIt->first;
        int movieCount = acIt->second;

        if (movieCount < 3)
        {
            continue;
        }

        double avg = actorRatingSum[token] / movieCount;

        ActorTrend at;
        at.token = actorDisplayName(token);
        at.avgRating = avg;
        at.movieCount = movieCount;

        data.topActors.push_back(at);
    }

    sort(data.topActors.begin(), data.topActors.end(), compareActors);

    data.ready = true;
    return data;
}

void showCurrentTrends(const TrendsData &trends)
{
    if (!trends.ready)
    {
        cout << "Trend data not yet computed.\n";
        return;
    }

    cout << "\n";
    cout << "====================================================\n";
    cout << "              CURRENT TRENDINGS\n";
    cout << "====================================================\n";

    cout << "\n  TOP GENRES BY POPULARITY\n";
    cout << "  " << string(50, '-') << "\n";

    int showGenres = min(10, (int)trends.genres.size());
    double maxPct = 1.0;

    if (!trends.genres.empty())
    {
        maxPct = trends.genres[0].popularityPct;
    }

    for (int i = 0; i < showGenres; i++)
    {
        const GenreTrend &g = trends.genres[i];

        int barLen = 0;
        if (maxPct > 0.0)
        {
            barLen = (int)(g.popularityPct / maxPct * 20.0);
        }

        barLen = max(1, barLen);

        string bar(barLen, '#');

        cout << "  " << setw(2) << (i + 1) << ". "
             << left << setw(18) << g.displayName
             << right
             << " [" << left << setw(20) << bar << right << "] "
             << fixed << setprecision(1) << g.popularityPct << "%"
             << "  avg " << setprecision(2) << g.avgRating << "/5\n";
    }

    cout << "\n  TOP 10 MOVIES RIGHT NOW\n";
    cout << "  " << string(50, '-') << "\n";

    int showMovies = min(10, (int)trends.topMovies.size());

    for (int i = 0; i < showMovies; i++)
    {
        const MovieTrend &m = trends.topMovies[i];

        string genreTag = m.primaryGenre;

        for (int j = 0; j < (int)GENRE_DISPLAY.size(); j++)
        {
            if (GENRE_DISPLAY[j].first == m.primaryGenre)
            {
                genreTag = GENRE_DISPLAY[j].second;
                break;
            }
        }

        int fullStars = max(0, min(5, (int)round(m.bayesianAvg)));
        string stars(fullStars, '*');
        string empty(5 - fullStars, '.');

        string titleDisplay;
        if ((int)m.title.size() > 33)
        {
            titleDisplay = m.title.substr(0, 30) + "...";
        }
        else
        {
            titleDisplay = m.title;
        }

        cout << "  " << setw(2) << (i + 1) << ". "
             << left << setw(34) << titleDisplay
             << right
             << " " << stars << empty
             << " " << fixed << setprecision(2) << m.bayesianAvg
             << "  [" << genreTag << "]"
             << "  (" << m.ratingCount << " ratings)\n";
    }

    cout << "\n  POPULAR ACTORS  (derived from dataset actor-position tags)\n";
    cout << "  " << string(50, '-') << "\n";

    int showActors = min(10, (int)trends.topActors.size());

    for (int i = 0; i < showActors; i++)
    {
        const ActorTrend &a = trends.topActors[i];

        string display;
        if ((int)a.token.size() > 23)
        {
            display = a.token.substr(0, 23);
        }
        else
        {
            display = a.token;
        }

        cout << "  " << setw(2) << (i + 1) << ". "
             << left << setw(25) << display
             << right
             << " avg " << fixed << setprecision(2) << a.avgRating << "/5"
             << "  in " << a.movieCount << " movies\n";
    }

    cout << "\n====================================================\n";
}

vector<int> getTrendingMoviesByGenre(
    const string &genre,
    const TrendsData &trends,
    int topN,
    double threshold)
{
    string g = genre;

    transform(
        g.begin(),
        g.end(),
        g.begin(),
        [](unsigned char c)
        {
            return (char)tolower(c);
        });

    g.erase(remove(g.begin(), g.end(), ' '), g.end());

    vector<int> result;
    result.reserve(topN);

    for (int i = 0; i < (int)trends.topMovies.size(); i++)
    {
        if ((int)result.size() >= topN)
        {
            break;
        }

        if (trends.topMovies[i].primaryGenre == g)
        {
            result.push_back(trends.topMovies[i].movieId);
        }
    }

    if ((int)result.size() >= topN)
    {
        return result;
    }

    unordered_set<int> already(result.begin(), result.end());

    for (int i = 0; i < (int)trends.topMovies.size(); i++)
    {
        if ((int)result.size() >= topN)
        {
            break;
        }

        int movieId = trends.topMovies[i].movieId;

        if (already.count(movieId))
        {
            continue;
        }

        unordered_map<string, double>::const_iterator wit =
            trends.topMovies[i].genreWeights.find(g);

        if (wit != trends.topMovies[i].genreWeights.end() && wit->second >= threshold)
        {
            result.push_back(movieId);
            already.insert(movieId);
        }
    }

    return result;
}

vector<string> getTopTrendingGenres(const TrendsData &trends, int topN)
{
    vector<string> result;
    result.reserve(topN);

    for (int i = 0; i < topN && i < (int)trends.genres.size(); i++)
    {
        result.push_back(trends.genres[i].genre);
    }

    return result;
}