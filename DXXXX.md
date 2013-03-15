# Extending `std::search` to use Additional Searching Algorithms

Document #: DXXXXXX
Marshall Clow 	mclow.lists@gmail.com

_Note: This is an update of [n3411](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3411.pdf), presented in Portland. The major difference is a different interface for the search functions._

## Overview

`std::search` is a powerful tool for searching sequences, but there are lots of other search algorithms in the literature. For specialized tasks, some of them perform significantly better than `std::search`. In general, they do this by precomputing statistics about the pattern to be searched for, betting that this time can be made up during the search.

The basic principle is to break the search operation into two parts; the first part creates a "search object", which is specific to the pattern being searched for, and then the search object is passed, along with the data being searched, to `std::search`.

This is done by adding an additional overload to `std::search`, and some additional functions to create the search objects.

Two additional search algorithms are proposed for inclusion into the standard: "Boyer-Moore" and "Boyer-Moore-Horspool". Additionally, the interface for the search objects is documented so that library implementers and end users can create their own search objects and use them with `std::search`.

_Terminology: I will refer to the sequence that is being searched as the "corpus" and the sequence being searched for as the "pattern"._

## Search Objects

A search object is an object that is passed a pattern to search for in its constructor. Then, the `operator ()` is called to search a sequence for matches.
A search object that used a very simple search pattern might be:

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
				//	Is this a full match?
					if ( std::equal ( first_, last_, cFirst ))
						return cFirst;
					++cFirst;
					}
				return cLast;	// didn't find anything
				}

		private:
			Iterator first_;
			Iterator last_;
			};

Search objects should be Moveable and Assignable, but not DefaultConstructible.

## Algorithms

### Default Search and Default Search with Predicate

These are convienience functions that allow users of the new interface to use the existing functionality of `std::search`.  There are two of them; one that takes a predicate for comparison, the other uses `operator ==` for comparison.

These algorithms require that both the corpus and the pattern be represented with forward iterators (or better).

### Boyer-Moore

The Boyer–Moore string search algorithm is a particularly efficient string searching algorithm, and it has been the standard benchmark for the practical string search literature. The Boyer-Moore algorithm was invented by Bob Boyer and J. Strother Moore, and published in the October 1977 issue of the Communications of the ACM, and [a copy of that article is available]( http://www.cs.utexas.edu/~moore/publications/fstrpos.pdf); another description is available on [Wikipedia](http://en.wikipedia.org/wiki/Boyer%E2%80%93Moore_string_search_algorithm).

The Boyer-Moore algorithm uses two precomputed tables to give better performance than a naive search. These tables depend on the pattern being searched for, and give the Boyer-Moore algorithm a larger memory footprint and startup costs than a simpler algorithm, but these costs are recovered quickly during the searching process, especially if the pattern is longer than a few elements. Both tables contain only non-negative integers.

In the following discussion, I will refer to `pattern_length`, the length of the pattern being searched for; in other words, `std::distance ( pattern_first, pattern_last )`.

The first table contains one entry for each element in the "alphabet" being searched (i.e, the corpus). For searching (narrow) character sequences, a 256-element array can be used for quick lookup. For searching other types of data, a hash table can be used to save space. The contents of the first table are: For each element in the "alphabet" being processed (i.e, the set of all values contained in the corpus) If the element does not appear in the pattern, then `pattern_length`, otherwise `pattern_length - j`, where `j` is the maximum value for which `*(pattern_first + j) == element`.

_Note: Even though the table contains one entry for each element that occurs in the corpus, the contents of the tables only depend on the pattern._

The second table contains one entry for each element in the pattern; a `std::vector<pattern_length>` works well. Each entry in the table is basically the amount that the matching window can be moved when a mismatch is found.
The Boyer-Moore algorithm works by at each position, comparing an element in the pattern to one in the corpus. If it matches, it advances to the next element in both the pattern and the corpus. If the end of the pattern is reached, then a match has been found, and can be returned. If the elements being compared do not match, then the precomputed tables are consulted to determine where to position the pattern in the corpus, and what position in the pattern to resume the matching.

The Boyer-Moore algorithm requires that both the corpus and the pattern be represented with random-access iterators.

### Boyer-Moore-Horspool

The Boyer-Moore-Horspool search algorithm was published by Nigel Horspool in 1980. It is a refinement of the Boyer-Moore algorithm that trades space for time. It uses less space for internal tables than Boyer-Moore, and has poorer worst-case performance.

Like the Boyer-Moore algorithm, it has a table that (logically) contains one entry for each element in the pattern "alphabet". When a mismatch is found in the comparison between the pattern and the corpus, this table and the mismatched character in the corpus are used to decide how far to advance the pattern to start the new comparison.

A reasonable description (with sample code) is available on [Wikipedia](http://en.wikipedia.org/wiki/Boyer%E2%80%93Moore%E2%80%93Horspool_algorithm).

The Boyer-Moore-Horspool algorithm requires that both the corpus and the pattern be represented with random-access iterators.

### Calls to be added to the standard library

		template <typename Iterator, typename Searcher>
		Iterator search ( Iterator first, Iterator last, Searcher &&searcher );

		template <typename Iterator>
		_unspecified_ make_searcher ( Iterator first, Iterator last );

		template <typename Iterator, typename Pred>
		_unspecified_ make_searcher ( Iterator first, Iterator last, Pred p );

		template <typename RandomAccessIterator>
		_unspecified_ make_boyer_moore_searcher ( RandomAccessIterator first, RandomAccessIterator last );

		template <typename Iterator>
		_unspecified_ make_boyer_moore_horspool_searcher ( RandomAccessIterator first, RandomAccessIterator last );
		

### Example usage

		// existing code
		iter = search ( corpus_first, corpus_last, pattern_first, pattern_last );
		
		// same code, new interface
		iter = search ( corpus_first, corpus_last, make_searcher ( pattern_first, pattern_last ));
		
		// same code, different algorithm
		iter = search ( corpus_first, corpus_last, make_boyer_moore_searcher ( pattern_first, pattern_last ));
		
### Performance

Using the new interface with the existing search algorithms should fulfill all the performance guarantees for the current interface of `std::search`. No additional comparisons are necessary. However, the creation of the search object may add some additional overhead. Different algorithms will have different amounts of overhead to create the search object. The `default_search` objects, for example, should be cheap to create - they will typically be a pair of iterators. The Boyer-Moore search object, on the other hand, will contain a pair of tables, and require a significant amount of computation to create.

In [my tests](https://github.com/mclow/search-library), on medium sized search patterns (about 100 entries), the Boyer-Moore and Boyer-Moore-Horspool were about 8-10x faster than `std::search`. For longer patterns, the advantage increases. For short patterns, they may actually be slower. 

#### Test timings

These results were run on a MacBookPro (i5) computer, compiled with `clang 3.2 -O3`, and compared against libc++.  The corpus was about 2.8MB of base64-encoded text. All timings are normalized against `std::search`.

The titles of the test indicate where the pattern is located in the corpus being searched; "At Start", etc. "Not found" is the case where the pattern does not exist in the corpus, i.e, the search will fail.

<table border=1>
    <tr><th>Algorithm</th><th>At Start</th><th>Middle</th><th>At End</th><th>Not Found</th></tr>
<tr><td><I>Pattern Length</I></td><td>119</td><td>105</td><td>43</td><td>91</td></tr>
    <tr><td>std::search</td><td>100.0</td><td>100.0</td><td>100.0</td><td>100.0</td></tr>
    <tr><td>default search</td><td>107.1</td><td>92.79</td><td>104.6</td><td>107</td></tr>
    <tr><td>Boyer-Moore</td><td>110.7</td><td>14.34</td><td>23.14</td><td>12.86</td></tr>
    <tr><td>Boyer-Moore Horspool</td><td>82.14</td><td>11.8</td><td>20.04</td><td>10.41</td></tr>
</table>

### Sample Implementation

An implementation of this proposal, available under the [Boost Software License](http://www.boost.org/LICENSE_1_0.txt) can be found on [GitHub](https://github.com/mclow/search-library).
