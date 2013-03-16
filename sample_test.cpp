/*
 (c) Copyright Marshall Clow 2013.

 Distributed under the Boost Software License, Version 1.0.
 http://www.boost.org/LICENSE_1_0.txt
*/

#include "searching.hpp"

#include <string>
#include <iostream>

		template <typename Iterator>
		class sample_searcher {
		public:
			sample_searcher ( Iterator first, Iterator last ) :
				first_ ( first ), last_ ( last ) {}

			template <typename Iterator2>
			Iterator2 operator () ( Iterator2 cFirst, Iterator2 cLast ) const {
				if ( first_ == last_ ) return cFirst;	// empty pattern
				if ( cFirst == cLast ) return cLast;	// empty corpus
				while ( cFirst != cLast ) {
				//	Advance to the first match
					while ( *cFirst != *first_ ) {
						++cFirst;
						if ( cFirst == cLast )
							return cLast;
					}
				//	At this point, we know that the first element matches
				//	and neither the corpus nor pattern are empty
					Iterator it1 = first_;
					Iterator2 it2 = cFirst;
					while ( it1 != last_ && it2 != cLast && *++it1 == *++it2 )
						;
					if ( it1 == last_ ) // we matched the whole pattern
						return cFirst;
					++cFirst;
					}
				return cLast;	// didn't find anything
				}

		private:
			Iterator first_;
			Iterator last_;
			};




template<typename Container>
void check_one ( const Container &haystack, const std::string &needle, int expected ) {
	typedef typename Container::const_iterator iter_type;
	typedef std::string::const_iterator pattern_type;
		
	iter_type	 hBeg = haystack.begin ();
	iter_type	 hEnd = haystack.end ();
	pattern_type nBeg = needle.begin ();
	pattern_type nEnd = needle.end ();

	iter_type it0  = std::search ( hBeg, hEnd, nBeg, nEnd );
	iter_type it1  = tba::search ( hBeg, hEnd, sample_searcher<pattern_type> ( nBeg, nEnd ));

	if ( it0 != it1 ) {
		std::cerr << "## Mismatch between std::search and sample_search " 
				  << std::distance ( hBeg, it0 ) << " vs " << std::distance ( hBeg, it1 ) << std::endl;
		}
	}


int main ( int argc, char *argv[] ) {
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
