/*
 (c) Copyright Marshall Clow 2013.

 Distributed under the Boost Software License, Version 1.0.
 http://www.boost.org/LICENSE_1_0.txt
*/

#include <algorithm>
#include <exception>
#include <vector>
#include <array>
#include <unordered_map>
#include <cassert>
#include <type_traits>

namespace tba {

template <typename Iterator, typename Searcher>
Iterator search ( Iterator first, Iterator last, Searcher &&searcher ) {
	return searcher ( first, last );
	}

namespace detail {

	template <typename Iterator>
	class default_searcher {
	public:
		default_searcher ()							               = delete;
		default_searcher ( Iterator first, Iterator last ) :
			first_ ( first ), last_ ( last ) {}
		default_searcher ( const default_searcher & )              = default;
		default_searcher ( default_searcher && )	               = default;
		default_searcher & operator = ( const default_searcher & ) = default;
		default_searcher & operator = ( default_searcher && )	   = default;
		~default_searcher ()						               = default;
	
		template <typename CorpusIterator>
		CorpusIterator operator () ( CorpusIterator cFirst, CorpusIterator cLast ) const {
			return std::search ( cFirst, cLast, first_, last_ );
			}
	
	private:
		Iterator first_;
		Iterator last_;
		};


	template <typename Iterator, typename Pred>
	class default_searcher_with_predicate {
	public:
		default_searcher_with_predicate ()							  = delete;
		default_searcher_with_predicate ( Iterator first, Iterator last, Pred p ) :
			first_ ( first ), last_ ( last ), p_ (p) {}
		default_searcher_with_predicate ( const default_searcher_with_predicate & )            = default;
		default_searcher_with_predicate ( default_searcher_with_predicate && )	               = default;
		default_searcher_with_predicate & operator = ( const default_searcher_with_predicate & ) = default;
		default_searcher_with_predicate & operator = ( default_searcher_with_predicate && )	   = default;
		~default_searcher_with_predicate ()						               = default;
	
		template <typename CorpusIterator>
		CorpusIterator operator () ( CorpusIterator cFirst, CorpusIterator cLast ) const {
			return std::search ( cFirst, cLast, first_, last_, p_ );
			}
	
	private:
		Iterator first_;
		Iterator last_;
		Pred p_;
		};


//
//  Default implementations of the skip tables for B-M and B-M-H
//
    template<typename key_type, typename value_type, bool /*useArray*/> class skip_table;

//  General case for data searching other than bytes; use a map
    template<typename key_type, typename value_type>
    class skip_table<key_type, value_type, false> {
    private:
        const value_type k_default_value;
        std::unordered_map<key_type, value_type> skip_;
        
    public:
        skip_table () = delete;
        skip_table ( std::size_t patSize, value_type default_value ) 
            : k_default_value ( default_value ), skip_ ( patSize ) {}
        
        void insert ( key_type key, value_type val ) {
            skip_ [ key ] = val;    // Would skip_.insert (val) be better here?
            }

        value_type operator [] ( key_type key ) const {
            auto it = skip_.find ( key );
            return it == skip_.end () ? k_default_value : it->second;
            }
        };
        
    
//  Special case small numeric values; use an array
    template<typename key_type, typename value_type>
    class skip_table<key_type, value_type, true> {
    private:
        typedef typename std::make_unsigned<key_type>::type unsigned_key_type;
        typedef std::array<value_type, 1U << (CHAR_BIT * sizeof(key_type))> skip_map;
        skip_map skip_;
        const value_type k_default_value;
    public:
        skip_table ( std::size_t patSize, value_type default_value ) : k_default_value ( default_value ) {
            std::fill_n ( skip_.begin(), skip_.size(), default_value );
            }
        
        void insert ( key_type key, value_type val ) {
            skip_ [ static_cast<unsigned_key_type> ( key ) ] = val;
            }

        value_type operator [] ( key_type key ) const {
            return skip_ [ static_cast<unsigned_key_type> ( key ) ];
            }
        };

    template<typename Iterator>
    struct BM_traits {
        typedef typename std::iterator_traits<Iterator>::difference_type value_type;
        typedef typename std::iterator_traits<Iterator>::value_type key_type;
        typedef skip_table<key_type, value_type, 
                std::is_integral<key_type>::value && (sizeof(key_type)==1)> skip_table_t;
        };


    template <typename patIter, typename traits = BM_traits<patIter>>
    class boyer_moore_searcher {
        typedef typename std::iterator_traits<patIter>::difference_type difference_type;
    public:
		boyer_moore_searcher ()							  = delete;
        boyer_moore_searcher ( patIter first, patIter last ) 
                : pat_first ( first ), pat_last ( last ),
                  k_pattern_length ( std::distance ( pat_first, pat_last )),
                  skip_ ( k_pattern_length, -1 ),
                  suffix_ ( k_pattern_length + 1 )
            {
            this->build_skip_table   ( first, last );
            this->build_suffix_table ( first, last );
            }
            
        ~boyer_moore_searcher () {}
        boyer_moore_searcher ( const boyer_moore_searcher &rhs ) = default;
        boyer_moore_searcher ( boyer_moore_searcher &&rhs ) = default;
        
        boyer_moore_searcher & operator = ( const boyer_moore_searcher &rhs ) = default;
        boyer_moore_searcher & operator = ( boyer_moore_searcher &&rhs ) = default;

        /// \fn operator ( corpusIter corpus_first, corpusIter corpus_last )
        /// \brief Searches the corpus for the pattern that was passed into the constructor
        /// 
        /// \param corpus_first The start of the data to search (Random Access Iterator)
        /// \param corpus_last  One past the end of the data to search
        ///
        template <typename corpusIter>
        corpusIter operator () ( corpusIter corpus_first, corpusIter corpus_last ) const {
            static_assert ( std::is_same<
                    typename std::decay<typename std::iterator_traits<patIter>   ::value_type>::type, 
                    typename std::decay<typename std::iterator_traits<corpusIter>::value_type>::type
                    	>::value,
                    "Corpus and Pattern iterators must point to the same type" );

            if ( corpus_first == corpus_last ) return corpus_last;  // if nothing to search, we didn't find it!
            if (    pat_first ==    pat_last ) return corpus_first; // empty pattern matches at start

            const difference_type k_corpus_length  = std::distance ( corpus_first, corpus_last );
        //  If the pattern is larger than the corpus, we can't find it!
            if ( k_corpus_length < k_pattern_length ) 
                return corpus_last;

        //  Do the search 
            return this->do_search   ( corpus_first, corpus_last );
            }
            
    private:
        patIter pat_first, pat_last;
        const difference_type k_pattern_length;
        typename traits::skip_table_t skip_;
        std::vector <difference_type> suffix_;

        /// \fn operator ( corpusIter corpus_first, corpusIter corpus_last, Pred p )
        /// \brief Searches the corpus for the pattern that was passed into the constructor
        /// 
        /// \param corpus_first The start of the data to search (Random Access Iterator)
        /// \param corpus_last  One past the end of the data to search
        /// \param p            A predicate used for the search comparisons.
        ///
        template <typename corpusIter>
        corpusIter do_search ( corpusIter corpus_first, corpusIter corpus_last ) const {
        /*  ---- Do the matching ---- */
            corpusIter curPos = corpus_first;
            const corpusIter lastPos = corpus_last - k_pattern_length;
            difference_type j, k, m;

            while ( curPos <= lastPos ) {
        /*  while ( std::distance ( curPos, corpus_last ) >= k_pattern_length ) { */
            //  Do we match right where we are?
                j = k_pattern_length;
                while ( pat_first [j-1] == curPos [j-1] ) {
                    j--;
                //  We matched - we're done!
                    if ( j == 0 )
                        return curPos;
                    }
                
            //  Since we didn't match, figure out how far to skip forward
                k = skip_ [ curPos [ j - 1 ]];
                m = j - k - 1;
                if ( k < j && m > suffix_ [ j ] )
                    curPos += m;
                else
                    curPos += suffix_ [ j ];
                }
        
            return corpus_last;     // We didn't find anything
            }


        void build_skip_table ( patIter first, patIter last ) {
            for ( std::size_t i = 0; first != last; ++first, ++i )
                skip_.insert ( *first, i );
            }
        

        template<typename Iter, typename Container>
        void compute_bm_prefix ( Iter pat_first, Iter pat_last, Container &prefix ) {
            const std::size_t count = std::distance ( pat_first, pat_last );
            assert ( count > 0 );
            assert ( prefix.size () == count );
                            
            prefix[0] = 0;
            std::size_t k = 0;
            for ( std::size_t i = 1; i < count; ++i ) {
                assert ( k < count );
                while ( k > 0 && ( pat_first[k] != pat_first[i] )) {
                    assert ( k < count );
                    k = prefix [ k - 1 ];
                    }
                    
                if ( pat_first[k] == pat_first[i] )
                    k++;
                prefix [ i ] = k;
                }
            }

        void build_suffix_table ( patIter pat_first, patIter pat_last ) {
            const std::size_t count = (std::size_t) std::distance ( pat_first, pat_last );
            
            if ( count > 0 ) {  // empty pattern
                std::vector<typename std::iterator_traits<patIter>::value_type> reversed(count);
                (void) std::reverse_copy ( pat_first, pat_last, reversed.begin ());
                
                std::vector<difference_type> prefix (count);
                compute_bm_prefix ( pat_first, pat_last, prefix );
        
                std::vector<difference_type> prefix_reversed (count);
                compute_bm_prefix ( reversed.begin (), reversed.end (), prefix_reversed );
                
                for ( std::size_t i = 0; i <= count; i++ )
                    suffix_[i] = count - prefix [count-1];
         
                for ( std::size_t i = 0; i < count; i++ ) {
                    const std::size_t     j = count - prefix_reversed[i];
                    const difference_type k = i -     prefix_reversed[i] + 1;
         
                    if (suffix_[j] > k)
                        suffix_[j] = k;
                    }
                }
            }
        };


    template <typename patIter, typename traits = BM_traits<patIter>>
    class boyer_moore_horspool_searcher {
        typedef typename std::iterator_traits<patIter>::difference_type difference_type;
    public:
		boyer_moore_horspool_searcher ()							  = delete;
        boyer_moore_horspool_searcher ( patIter first, patIter last ) 
                : pat_first ( first ), pat_last ( last ),
                  k_pattern_length ( std::distance ( pat_first, pat_last )),
                  skip_ ( k_pattern_length, k_pattern_length ) {
                  
            std::size_t i = 0;
            if ( first != last )    // empty pattern?
                for ( patIter iter = first; iter != last-1; ++iter, ++i )
                    skip_.insert ( *iter, k_pattern_length - 1 - i );
            }
            
        boyer_moore_horspool_searcher ( const boyer_moore_horspool_searcher &rhs ) = default;
        boyer_moore_horspool_searcher (       boyer_moore_horspool_searcher &&rhs ) = default;
        
        boyer_moore_horspool_searcher & operator = ( const boyer_moore_horspool_searcher &rhs ) = default;
        boyer_moore_horspool_searcher & operator = (       boyer_moore_horspool_searcher &&rhs ) = default;

            
        ~boyer_moore_horspool_searcher () {}
        
        /// \fn operator ( corpusIter corpus_first, corpusIter corpus_last, Pred p )
        /// \brief Searches the corpus for the pattern that was passed into the constructor
        /// 
        /// \param corpus_first The start of the data to search (Random Access Iterator)
        /// \param corpus_last  One past the end of the data to search
        /// \param p            A predicate used for the search comparisons.
        ///
        template <typename corpusIter>
        corpusIter operator () ( corpusIter corpus_first, corpusIter corpus_last ) const {
            static_assert ( std::is_same<
                    typename std::decay<typename std::iterator_traits<patIter>   ::value_type>::type, 
                    typename std::decay<typename std::iterator_traits<corpusIter>::value_type>::type
                    	>::value,
                    "Corpus and Pattern iterators must point to the same type" );

            if ( corpus_first == corpus_last ) return corpus_last;  // if nothing to search, we didn't find it!
            if (    pat_first ==    pat_last ) return corpus_first; // empty pattern matches at start

            const difference_type k_corpus_length  = std::distance ( corpus_first, corpus_last );
        //  If the pattern is larger than the corpus, we can't find it!
            if ( k_corpus_length < k_pattern_length )
                return corpus_last;
    
        //  Do the search 
            return this->do_search ( corpus_first, corpus_last );
            }

    private:
        patIter pat_first, pat_last;
        const difference_type k_pattern_length;
        typename traits::skip_table_t skip_;

        /// \fn do_search ( corpusIter corpus_first, corpusIter corpus_last )
        /// \brief Searches the corpus for the pattern that was passed into the constructor
        /// 
        /// \param corpus_first The start of the data to search (Random Access Iterator)
        /// \param corpus_last  One past the end of the data to search
        /// \param k_corpus_length The length of the corpus to search
        ///
        template <typename corpusIter>
        corpusIter do_search ( corpusIter corpus_first, corpusIter corpus_last ) const {
            corpusIter curPos = corpus_first;
            const corpusIter lastPos = corpus_last - k_pattern_length;
            while ( curPos <= lastPos ) {
            //  Do we match right where we are?
                std::size_t j = k_pattern_length - 1;
                while ( pat_first [j] == curPos [j] ) {
                //  We matched - we're done!
                    if ( j == 0 )
                        return curPos;
                    j--;
                    }
        
                curPos += skip_ [ curPos [ k_pattern_length - 1 ]];
                }
            
            return corpus_last;
            }
        };
    
	}

template <typename Iterator>
detail::default_searcher<Iterator> make_searcher ( Iterator first, Iterator last ) {
	return detail::default_searcher<Iterator> ( first, last );
	}

template <typename Iterator, typename Pred>
detail::default_searcher_with_predicate<Iterator, Pred> make_searcher ( Iterator first, Iterator last, Pred p ) {
	return detail::default_searcher_with_predicate<Iterator, Pred> ( first, last, p );
	}

template <typename Iterator, typename traits=detail::BM_traits<Iterator>>
detail::boyer_moore_searcher<Iterator, traits> make_boyer_moore_searcher ( Iterator first, Iterator last ) {
	return detail::boyer_moore_searcher<Iterator, traits> ( first, last );
	}

template <typename Iterator, typename traits=detail::BM_traits<Iterator>>
detail::boyer_moore_horspool_searcher<Iterator, traits> make_boyer_moore_horspool_searcher ( Iterator first, Iterator last ) {
	return detail::boyer_moore_horspool_searcher<Iterator, traits> ( first, last );
	}

}