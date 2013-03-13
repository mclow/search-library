/*
 (c) Copyright Marshall Clow 2013.

 Distributed under the Boost Software License, Version 1.0.
 http://www.boost.org/LICENSE_1_0.txt
*/

#include "searching.hpp"

#include <string>
#include <iostream>
#include <iomanip>		// for setprecision
#include <ctime>        // for clock_t
#include <fstream>
#include <chrono>
#include <random>

#define	CORPUS_SIZE	3000000

typedef std::chrono::microseconds duration;

struct stats {
public:
	stats () {}
	~stats () {}
	template <typename T>
	void add ( T t ) { values_.push_back ( static_cast<double>(t)); }
	double count () const { return values_.size (); }
	double mean ()  const { return std::accumulate ( values_.begin (), values_.end (), 0.0) / values_.size (); }
	std::pair<double, double> minmax () const {
		auto mm = std::minmax_element ( values_.begin(), values_.end ());
		return std::make_pair ( *mm.first, *mm.second );
		}
	
private:
	std::vector<double> values_;
	};

template <typename T>
struct my_equals {
	bool operator () ( const T &one, const T &two ) const { return one == two; }
	};

template<typename Iterator>
struct map_BM_traits {
	typedef typename std::iterator_traits<Iterator>::difference_type value_type;
	typedef typename std::iterator_traits<Iterator>::value_type key_type;
	typedef tba::detail::skip_table<key_type, value_type, false> skip_table_t;
	};


template <typename Container, typename NeedleIter>
int OverAndOver ( const Container &haystack, NeedleIter nBegin, NeedleIter nEnd ) {
	typename Container::const_iterator ret;
	for ( int i = 0; i < 200; ++i )
		ret = std::search ( haystack.begin(), haystack.end (), nBegin, nEnd );
	return ret == haystack.end () ? -1 : std::distance ( haystack.begin(), ret);
	}

template <typename Container, typename Searcher>
int OverAndOver ( const Container &haystack, Searcher && searcher ) {
	typename Container::const_iterator ret;
	for ( int i = 0; i < 200; ++i )
		ret = tba::search ( haystack.begin(), haystack.end (), searcher );
	return ret == haystack.end () ? -1 : std::distance ( haystack.begin(), ret);
	}

template <typename Container>
duration std_search ( const Container &haystack, const Container &needle, int expected ) {
	auto start = std::chrono::high_resolution_clock::now ();
	int ret = OverAndOver ( haystack, needle.begin (), needle.end ());
	duration elapsed = std::chrono::duration_cast<duration> ( std::chrono::high_resolution_clock::now () - start );
	if ( ret != expected )
		std::cerr << "Unexpected return from std_searcher; got " << ret << ", expected " << expected << std::endl;
	return elapsed;
	}


template <typename Container>
duration default_search ( const Container &haystack, const Container &needle, int expected ) {
	auto start = std::chrono::high_resolution_clock::now ();
	int ret = OverAndOver ( haystack, tba::make_searcher ( needle.begin (), needle.end ()));
	duration elapsed = std::chrono::duration_cast<duration> ( std::chrono::high_resolution_clock::now () - start );
	if ( ret != expected )
		std::cerr << "Unexpected return from default_searcher; got " << ret << ", expected " << expected << std::endl;
	return elapsed;
	}

template <typename Container>
duration default_search_p ( const Container &haystack, const Container &needle, int expected ) {
	auto start = std::chrono::high_resolution_clock::now ();
	int ret = OverAndOver ( haystack, tba::make_searcher ( needle.begin (), needle.end ()), my_equals<typename Container::value_type>());
	duration elapsed = std::chrono::duration_cast<duration> ( std::chrono::high_resolution_clock::now () - start );
	if ( ret != expected )
		std::cerr << "Unexpected return from default_searcher; got " << ret << ", expected " << expected << std::endl;
	return elapsed;
	}

template <typename Container>
duration bm_search ( const Container &haystack, const Container &needle, int expected ) {
	auto start = std::chrono::high_resolution_clock::now ();
	int ret = OverAndOver ( haystack, tba::make_boyer_moore_searcher ( needle.begin (), needle.end ()));
	duration elapsed = std::chrono::duration_cast<duration> ( std::chrono::high_resolution_clock::now () - start );
	if ( ret != expected )
		std::cerr << "Unexpected return from boyer_moore; got " << ret << ", expected " << expected << std::endl;
	return elapsed;
	}

// -----

template <typename Container>
duration bm_search_map ( const Container &haystack, const Container &needle, int expected ) {
	auto start = std::chrono::high_resolution_clock::now ();
	int ret = OverAndOver ( haystack, 
	       tba::make_boyer_moore_searcher<typename Container::const_iterator, map_BM_traits<typename Container::const_iterator>> ( needle.begin (), needle.end ()));
	duration elapsed = std::chrono::duration_cast<duration> ( std::chrono::high_resolution_clock::now () - start );
	if ( ret != expected )
		std::cerr << "Unexpected return from boyer_moore(map); got " << ret << ", expected " << expected << std::endl;
	return elapsed;
	}

// -----

template <typename Container>
duration bmh_search ( const Container &haystack, const Container &needle, int expected ) {
	auto start = std::chrono::high_resolution_clock::now ();
	int ret = OverAndOver ( haystack, tba::make_boyer_moore_horspool_searcher ( needle.begin (), needle.end ()));
	duration elapsed = std::chrono::duration_cast<duration> ( std::chrono::high_resolution_clock::now () - start );
	if ( ret != expected )
		std::cerr << "Unexpected return from boyer_moore; got " << ret << ", expected " << expected << std::endl;
	return elapsed;
	}


// -----

template <typename Container>
duration bmh_search_map ( const Container &haystack, const Container &needle, int expected ) {
	auto start = std::chrono::high_resolution_clock::now ();
	int ret = OverAndOver ( haystack, 
	       tba::make_boyer_moore_horspool_searcher<typename Container::const_iterator, map_BM_traits<typename Container::const_iterator>> ( needle.begin (), needle.end ()));
	duration elapsed = std::chrono::duration_cast<duration> ( std::chrono::high_resolution_clock::now () - start );
	if ( ret != expected )
		std::cerr << "Unexpected return from boyer_moore(map); got " << ret << ", expected " << expected << std::endl;
	return elapsed;
	}

// -----

template <typename T>
double pct ( T whole, T part ) { return 100 * double (part) / double (whole); }

template <typename T>
double dur_pct ( T whole, T part ) { return 100 * double (part.count ()) / double (whole.count ()); }

typedef struct {
	stats stds;
	stats def;
	stats def_p;
	stats bm;
	stats bm_map;
	stats bmh;
	stats bmh_map;
	} RunStats;
	
RunStats gStats [4][3];	// six, 500, and 10K by [ start, middle, end, not ]

template <typename Container>
void check_one (  const Container &haystack, const Container &needle, int where, RunStats &st ) {
	int expected;
	switch ( where ) {
		case -2: {
					auto it = std::search ( haystack.begin (), haystack.end (), needle.begin (), needle.end ());
					 assert ( it != haystack.end ());
					 expected = std::distance ( haystack.begin (), it);
				 }
				 break;
				 
		case -3: expected = haystack.size () - needle.size (); break;
		default: expected = where; break;
		}
	

//	std::cout << "Needle is " << needle.size () << " entries long\n";
	duration stds = std_search ( haystack, needle, expected );
//	std::cout << "Standard search took       :             " << stds.count ()        << "\t(" << dur_pct ( stds, stds ) << ")" << std::endl;
	duration def = default_search ( haystack, needle, expected );
//	std::cout << "Default search took       :             " << def.count ()        << "\t(" << dur_pct ( stds, def ) << ")" << std::endl;
	duration def_p = default_search ( haystack, needle, expected );
//	std::cout << "Default search w/pred took:             " << def_p.count ()      << "\t(" << dur_pct ( stds, def_p ) << ")" << std::endl;
	duration bm = bm_search ( haystack, needle, expected );
//	std::cout << "Boyer-Moore search took:                " << bm.count ()         << "\t(" << dur_pct ( stds, bm ) << ")" << std::endl;
	duration bm_map = bm_search_map ( haystack, needle, expected );
//	std::cout << "Boyer-Moore (map) search took:          " << bm_map.count ()     << "\t(" << dur_pct ( stds, bm_map ) << ")" << std::endl;
	duration bmh = bmh_search ( haystack, needle, expected );
//	std::cout << "Boyer-Moore-Horspool search took:       " << bmh.count ()        << "\t(" << dur_pct ( stds, bmh ) << ")" << std::endl;
	duration bmh_map = bmh_search_map ( haystack, needle, expected );
//	std::cout << "Boyer-Moore-Horspool (map) search took: " << bmh_map.count ()    << "\t(" << dur_pct ( stds, bmh_map ) << ")" << std::endl;

//	Add the stats into the accumulators
	st.stds.add    ( stds.count ());
	st.def.add     ( def.count ());
	st.def_p.add   ( def_p.count ());
	st.bm.add      ( bm.count ());
	st.bm_map.add  ( bm_map.count ());
	st.bmh.add     ( bmh.count ());
	st.bmh_map.add ( bmh_map.count ());
	}

//	Find me something that doesn't occur in the corpus
std::vector<char> 
find_mismatch ( std::mt19937 &rng, std::uniform_int_distribution<char> &dist, size_t count, const std::vector<char> &corpus ) {
	std::vector<char> ret;
	ret.reserve ( count );
	do {
		ret.clear ();
		for ( size_t i = 0; i < count; ++i )
			ret.push_back (dist(rng));
		} while ( std::search ( corpus.begin (), corpus.end (), ret.begin (), ret.end ()) != corpus.end ());
	return ret;
	}


void run_one_test () {
	std::mt19937 rng;
	rng.seed (std::clock ());
	std::uniform_int_distribution<char> dist;
	
	std::vector<char> corpus;
	for ( int i = 0; i < CORPUS_SIZE; ++i )
		corpus.push_back ( dist(rng));

//	std::cout << "--- Beginning ---" << std::endl;
	std::vector<char> b6, b500, b10K;
	std::copy_n ( corpus.begin (),     6, std::back_inserter (b6));
	std::copy_n ( corpus.begin (),   500, std::back_inserter (b500));
	std::copy_n ( corpus.begin (), 10000, std::back_inserter (b10K));
    check_one ( corpus, b6,   0, gStats[0][0] );       //  Find it at position zero
    check_one ( corpus, b500, 0, gStats[0][1] );
    check_one ( corpus, b10K, 0, gStats[0][2] );

//	std::cout << "--- Middle ---" << std::endl;
	std::vector<char> p6, p500, p10K;
	const size_t middle = CORPUS_SIZE / 2;
	static_assert ( middle + 10000 < CORPUS_SIZE, "CORPUS_SIZE too small" );
	std::copy_n ( corpus.begin () + middle,     6, std::back_inserter (p6));
	std::copy_n ( corpus.begin () + middle,   500, std::back_inserter (p500));
	std::copy_n ( corpus.begin () + middle, 10000, std::back_inserter (p10K));
    check_one ( corpus, p6,    middle, gStats[1][0] );       //  Find it at position  150000
    check_one ( corpus, p500,  middle, gStats[1][1] );
    check_one ( corpus, p10K,  middle, gStats[1][2] );

//	std::cout << "--- End ---" << std::endl;
	std::vector<char> e6, e500, e10K;
	std::copy_n ( corpus.end () -     6,     6, std::back_inserter (e6));
	std::copy_n ( corpus.end () -   500,   500, std::back_inserter (e500));
	std::copy_n ( corpus.end () - 10000, 10000, std::back_inserter (e10K));
    check_one ( corpus, e6,    -3, gStats[2][0] );       //  Find it at the end
    check_one ( corpus, e500,  -3, gStats[2][1] );
    check_one ( corpus, e10K,  -3, gStats[2][2] );

//	std::cout << "--- Not found ---" << std::endl;
	std::vector<char> n6   = find_mismatch ( rng, dist,     6, corpus );
	std::vector<char> n500 = find_mismatch ( rng, dist,   500, corpus );
	std::vector<char> n10K = find_mismatch ( rng, dist, 10000, corpus );
    check_one ( corpus, n6,    -1, gStats[3][0] );       //  Don't find it
    check_one ( corpus, n500,  -1, gStats[3][1] );
    check_one ( corpus, n10K,  -1, gStats[3][2] );
	}

const char *kSearch [] = { " start ", "middle ", "  end  ", "missing" };
const char *kLengths [] = { " 6 ", "500", "10K" };

void print ( int i, int j, const stats &st, double whole ) {
	auto mm = st.minmax ();
	std::cout << "[ " << kSearch[i] << ", " << kLengths[j] << " ]:"
		<< "\t(" << "min:" << mm.first << '\t' << "avg:" << st.mean () << '\t' << "max:" << mm.second << ")"
		<< "\t(% of default:" << pct (whole, st.mean ()) << ")" << std::endl;
	}
	
int main ( int argc, char *argv[] ) {
    std::cout << std::ios::fixed << std::setprecision(4);
	int count = 3;
	if ( argc == 2 )
		count = std::atoi ( argv[1] );
	std::cout << "Running " << count << " rounds" << std::endl;
	std::cout << "Corpus size = " << CORPUS_SIZE << std::endl;
	std::cout << std::endl;
	
	for ( int i = 0; i < count; ++i )
		run_one_test ();
	
	for ( int i = 0; i < 4; ++i )
		for ( int j = 0; j < 3; ++j )
			if ( gStats[i][j].def.count () != count )
				std::cerr << "## Accumulator count mismatch [" << i << "," << j << "]  " << gStats[i][j].def.count () << std::endl;


	for ( int i = 0; i < 4; ++i )
		for ( int j = 0; j < 3; ++j ) {
			double whole = gStats[i][j].stds.mean ();
			print ( i, j, gStats[i][j].stds,    whole );
			print ( i, j, gStats[i][j].def,     whole );
			print ( i, j, gStats[i][j].def_p,   whole );
			print ( i, j, gStats[i][j].bm,      whole );
			print ( i, j, gStats[i][j].bm_map,  whole );
			print ( i, j, gStats[i][j].bmh,     whole );
			print ( i, j, gStats[i][j].bmh_map, whole );
			std::cout << std::endl;
			}
			
	return 0;
	}
