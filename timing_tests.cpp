/*
 (c) Copyright Marshall Clow 2013.

 Distributed under the Boost Software License, Version 1.0.
 http://www.boost.org/LICENSE_1_0.txt
*/

#include "searching.hpp"

#include <string>
#include <iostream>
#include <iomanip>	// for setprecision
#include <fstream>
#include <chrono>

typedef std::chrono::microseconds duration;

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


template <typename vec>
vec ReadFromFile ( const char *name ) {
	std::ifstream in ( name, std::ios_base::binary | std::ios_base::in );
	vec retVal;
	std::istream_iterator<char, char> begin(in);
	std::istream_iterator<char, char> end;
	
	std::copy ( begin, end, std::back_inserter ( retVal ));
	return retVal;
	}


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
double dur_pct ( T whole, T part ) { return 100 * double (part.count ()) / double (whole.count ()); }

template <typename Container>
void check_one (  const Container &haystack, const Container &needle, int where ) {
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
	
    std::cout << "Needle is " << needle.size () << " entries long\n";
	duration stds = std_search ( haystack, needle, expected );
	std::cout << "Standard search took       :            " << stds.count ()   << "\t(" << dur_pct ( stds, stds ) << ")" << std::endl;
	duration def = default_search ( haystack, needle, expected );
	std::cout << "Default search took       :             " << def.count ()    << "\t(" << dur_pct ( stds, def ) << ")" << std::endl;
	duration def_p = default_search ( haystack, needle, expected );
	std::cout << "Default search w/pred took:             " << def_p.count ()  << "\t(" << dur_pct ( stds, def_p ) << ")" << std::endl;
	duration bm = bm_search ( haystack, needle, expected );
	std::cout << "Boyer-Moore search took:                " << bm.count ()     << "\t(" << dur_pct ( stds, bm ) << ")" << std::endl;
	duration bm_map = bm_search_map ( haystack, needle, expected );
	std::cout << "Boyer-Moore (map) search took:          " << bm_map.count () << "\t(" << dur_pct ( stds, bm_map ) << ")" << std::endl;
	duration bmh = bmh_search ( haystack, needle, expected );
	std::cout << "Boyer-Moore-Horspool search took:       " << bmh.count ()    << "\t(" << dur_pct ( stds, bmh ) << ")" << std::endl;
	duration bmh_map = bmh_search_map ( haystack, needle, expected );
	std::cout << "Boyer-Moore-Horspool (map) search took: " << bmh_map.count ()<< "\t(" << dur_pct ( stds, bmh_map ) << ")" << std::endl;
	}

int main ( int argc, char *argv[] ) {
    std::cout << std::ios::fixed << std::setprecision(4);

	typedef std::vector<char> vec;
    vec c1  = ReadFromFile<vec> ( "data/0001.corpus" );
        
    vec p0b { 'T', 'U', '0', 'A', 'K', 'g' };
    vec p0e { 'A', 'A', 'A', 'A', 'A', '=' };
    vec p0n { 'A', '0', 'z', 'q', 'T', '4' };
    vec p0f { 'F', 'h', 'X', 'V', 'k', 'x' };

    vec p1b = ReadFromFile<vec> ( "data/0001b.pat" );
    vec p1e = ReadFromFile<vec> ( "data/0001e.pat" );
    vec p1n = ReadFromFile<vec> ( "data/0001n.pat" );
    vec p1f = ReadFromFile<vec> ( "data/0001f.pat" );

    vec p2b = ReadFromFile<vec> ( "data/0002b.pat" );
    vec p2e = ReadFromFile<vec> ( "data/0002e.pat" );
    vec p2n = ReadFromFile<vec> ( "data/0002n.pat" );
    vec p2f = ReadFromFile<vec> ( "data/0002f.pat" );

    std::cout << "Corpus is " << c1.size () << " entries long\n";
    std::cout << "--- Beginning ---" << std::endl;
    check_one ( c1, p0b, 0 );       //  Find it at position zero
    check_one ( c1, p1b, 0 );
    check_one ( c1, p2b, 0 );
    std::cout << "---- Middle -----" << std::endl;
    check_one ( c1, p0f, -2 );      //  Don't know answer
    check_one ( c1, p1f, -2 );
    check_one ( c1, p2f, -2 );
    std::cout << "------ End ------" << std::endl;
    check_one ( c1, p0e, -3 );		// at the end
    check_one ( c1, p1e, -3 );
    check_one ( c1, p2e, -3 );
    std::cout << "--- Not found ---" << std::endl;
    check_one ( c1, p0n, -1 );      //  Not found
    check_one ( c1, p1n, -1 );
    check_one ( c1, p2n, -1 );

	return 0;
	}
