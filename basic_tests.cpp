/*
 (c) Copyright Marshall Clow 2013.

 Distributed under the Boost Software License, Version 1.0.
 http://www.boost.org/LICENSE_1_0.txt
*/

#include "searching.hpp"

#include <string>
#include <iostream>

template <typename T>
struct my_equals {
	bool operator () ( const T &one, const T &two ) const { return one == two; }
	};

//	Check using iterators
	template<typename Container>
	void check_one_iter ( const Container &haystack, const std::string &needle, int expected ) {
		typedef typename Container::const_iterator iter_type;
		typedef std::string::const_iterator pattern_type;
		
		iter_type	 hBeg = haystack.begin ();
		iter_type	 hEnd = haystack.end ();
		pattern_type nBeg = needle.begin ();
		pattern_type nEnd = needle.end ();

		iter_type it0  = std::search ( hBeg, hEnd, nBeg, nEnd );
		iter_type it1  = tba::search ( hBeg, hEnd, tba::make_searcher ( nBeg, nEnd ));
		iter_type it2  = tba::search ( hBeg, hEnd, tba::make_searcher ( nBeg, nEnd, my_equals<typename Container::value_type>()));
		iter_type it3  = tba::search ( hBeg, hEnd, tba::make_boyer_moore_searcher ( nBeg, nEnd ));
		iter_type it4  = tba::search ( hBeg, hEnd, tba::make_boyer_moore_horspool_searcher ( nBeg, nEnd ));
		const typename std::iterator_traits<iter_type>::difference_type dist = it1 == hEnd ? -1 : std::distance ( hBeg, it1 );

//		std::cout << "(Iterators) Pattern is " << needle.length () << ", haysstack is " << haystack.length () << " chars long; " << std::endl;
		try {
			if ( it0 != it1 ) {
				throw std::runtime_error ( 
					std::string ( "results mismatch between std::search and tba::search (default_searcher)" ));
				}

			if ( it0 != it2 ) {
				throw std::runtime_error ( 
					std::string ( "results mismatch between std::search and tba::search (default_searcher_p)" ));
				}

			if ( it0 != it3 ) {
				throw std::runtime_error ( 
					std::string ( "results mismatch between std::search and tba::search (bm_searcher)" ));
				}

			if ( it0 != it4 ) {
				throw std::runtime_error ( 
					std::string ( "results mismatch between std::search and tba::search (bmh_searcher)" ));
				}
			}

		catch ( ... ) {
			std::cout << "Searching for: " << needle << std::endl;
			std::cout << "Expected:   " << expected << "\n";
			std::cout << "	std:	  " << std::distance ( hBeg, it0 ) << "\n";
			std::cout << "	tba:	  " << std::distance ( hBeg, it1 ) << "\n";
			std::cout << "	tba(red): " << std::distance ( hBeg, it2 ) << "\n";
			std::cout << "	bm:	      " << std::distance ( hBeg, it3 ) << "\n";
			std::cout << "	bmh:      " << std::distance ( hBeg, it4 ) << "\n";
			std::cout << std::flush;
			throw ;
			}

		if ( dist != expected )
			std::cout << "## Unexpected result: " << dist << " instead of " << expected << std::endl;
		}


	template<typename Container>
	void check_one ( const Container &haystack, const std::string &needle, int expected ) {
		check_one_iter ( haystack, needle, expected );
		}


int main ( int, char ** ) {
	std::string haystack1 ( "NOW AN FOWE\220ER ANNMAN THE ANPANMANEND" );
	std::string needle1	  ( "ANPANMAN" );
	std::string needle2	  ( "MAN THE" );
	std::string needle3	  ( "WE\220ER" );
	std::string needle4	  ( "NOW " );	//	At the beginning
	std::string needle5	  ( "NEND" );	//	At the end
	std::string needle6	  ( "NOT FOUND" );	// Nowhere
	std::string needle7	  ( "NOT FO\340ND" );	// Nowhere

	std::string haystack2 ( "ABC ABCDAB ABCDABCDABDE" );
	std::string needle11  ( "ABCDABD" );

	std::string haystack3 ( "abra abracad abracadabra" );
	std::string needle12  ( "abracadabra" );

	std::string needle13   ( "" );
	std::string haystack4  ( "" );

	check_one ( haystack1, needle1, 26 );
	check_one ( haystack1, needle2, 18 );
	check_one ( haystack1, needle3,	 9 );
	check_one ( haystack1, needle4,	 0 );
	check_one ( haystack1, needle5, 33 );
	check_one ( haystack1, needle6, -1 );
	check_one ( haystack1, needle7, -1 );

	check_one ( needle1, haystack1, -1 );	// cant find long pattern in short corpus
	check_one ( haystack1, haystack1, 0 );	// find something in itself
	check_one ( haystack2, haystack2, 0 );	// find something in itself

	check_one ( haystack2, needle11, 15 );
	check_one ( haystack3, needle12, 13 );

	check_one ( haystack1, needle13, 0 );	// find the empty string 
	check_one ( haystack4, needle1, -1 );  // can't find in an empty haystack

//	Mikhail Levin <svarneticist@gmail.com> found a problem, and this was the test
//	that triggered it.

  const std::string mikhail_pattern =	
"GATACACCTACCTTCACCAGTTACTCTATGCACTAGGTGCGCCAGGCCCATGCACAAGGGCTTGAGTGGATGGGAAGGA"
"TGTGCCCTAGTGATGGCAGCATAAGCTACGCAGAGAAGTTCCAGGGCAGAGTCACCATGACCAGGGACACATCCACGAG"
"CACAGCCTACATGGAGCTGAGCAGCCTGAGATCTGAAGACACGGCCATGTATTACTGTGGGAGAGATGTCTGGAGTGGT"
"TATTATTGCCCCGGTAATATTACTACTACTACTACTACATGGACGTCTGGGGCAAAGGGACCACG"
;
	const std::string mikhail_corpus = std::string (8, 'a') + mikhail_pattern;

	check_one ( mikhail_corpus, mikhail_pattern, 8 );
	return 0;
	}
