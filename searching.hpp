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
#include <climits>

namespace tba {

template <typename Iterator, typename Searcher>
Iterator search ( Iterator first, Iterator last, const Searcher &searcher ) {
	return searcher ( first, last );
	}

	template <typename Iterator, typename BinaryPredicate = typename std::equal_to<typename std::iterator_traits<Iterator>::value_type>>
	class default_searcher {
	public:
		default_searcher ( Iterator first, Iterator last, BinaryPredicate pred = BinaryPredicate ()) :
			first_ ( first ), last_ ( last ), pred_ ( pred ) {}
	
		template <typename CorpusIterator>
		CorpusIterator operator () ( CorpusIterator cFirst, CorpusIterator cLast ) const {
			return std::search ( cFirst, cLast, first_, last_, pred_ );
			}
	
	private:
		Iterator first_;
		Iterator last_;
		BinaryPredicate pred_;
		};


//
//  Default implementations of the skip tables for B-M and B-M-H
//
    template<typename key_type, typename value_type, typename BinaryPredicate, bool /*useArray*/> class skip_table;

//  General case for data searching other than bytes; use a map
    template<typename key_type, typename value_type, typename BinaryPredicate>
    class skip_table<key_type, value_type, BinaryPredicate, false> {
    private:
        const value_type k_default_value;
        std::unordered_map<key_type, value_type, BinaryPredicate> skip_;
        
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
    template<typename key_type, typename value_type, typename BinaryPredicate>
    class skip_table<key_type, value_type, BinaryPredicate, true> {
    private:
        typedef typename std::make_unsigned<key_type>::type unsigned_key_type;
        typedef std::array<value_type, 1U << (CHAR_BIT * sizeof(key_type))> skip_map;
        skip_map skip_;
        const value_type k_default_value;
    public:
        skip_table ( std::size_t /*patSize*/, value_type default_value ) : k_default_value ( default_value ) {
            std::fill_n ( skip_.begin(), skip_.size(), default_value );
            }
        
        void insert ( key_type key, value_type val ) {
            skip_ [ static_cast<unsigned_key_type> ( key ) ] = val;
            }

        value_type operator [] ( key_type key ) const {
            return skip_ [ static_cast<unsigned_key_type> ( key ) ];
            }
        };

    template<typename Iterator, typename BinaryPredicate>
    struct BM_traits {
        typedef typename std::iterator_traits<Iterator>::difference_type value_type;
        typedef typename std::iterator_traits<Iterator>::value_type key_type;
        typedef skip_table<key_type, value_type, BinaryPredicate, 
                std::is_integral<key_type>::value && (sizeof(key_type)==1)> skip_table_t;
        };


    template <typename ForwardIterator, typename Hash, typename BinaryPredicate>
    class boyer_moore_searcher {
        typedef typename std::iterator_traits<ForwardIterator>::difference_type difference_type;
        typedef typename std::iterator_traits<ForwardIterator>::value_type      value_type;
    public:
        boyer_moore_searcher ( ForwardIterator first, ForwardIterator last, Hash hash, BinaryPredicate pred )
                : first_ ( first ), last_ ( last ), hash_ ( hash ), pred_ ( pred ),
                  k_pattern_length ( std::distance ( first_, last_ )),
                  skip_ ( k_pattern_length, hash_, pred_ ),
                  suffix_ ( k_pattern_length + 1 )
            {
            this->build_skip_table   ( first_, last_ );
            this->build_suffix_table ( first_, last_, pred_ );
            }
            
        /// \fn operator ( corpusIter corpus_first, corpusIter corpus_last )
        /// \brief Searches the corpus for the pattern that was passed into the constructor
        /// 
        /// \param corpus_first The start of the data to search (Random Access Iterator)
        /// \param corpus_last  One past the end of the data to search
        ///
        template <typename RandomAccessIterator>
        RandomAccessIterator 
        operator () ( RandomAccessIterator corpus_first, RandomAccessIterator corpus_last ) const {
            static_assert ( std::is_same<
                    typename std::decay<typename std::iterator_traits<ForwardIterator>     ::value_type>::type, 
                    typename std::decay<typename std::iterator_traits<RandomAccessIterator>::value_type>::type
                    	>::value,
                    "Corpus and Pattern iterators must point to the same type" );

            if ( corpus_first == corpus_last  ) return corpus_last;  // if nothing to search, we didn't find it!
            if (       first_ ==        last_ ) return corpus_first; // empty pattern matches at start

            const difference_type k_corpus_length  = std::distance ( corpus_first, corpus_last );
        //  If the pattern is larger than the corpus, we can't find it!
            if ( k_corpus_length < k_pattern_length ) 
                return corpus_last;

        //  Do the search 
            return this->do_search   ( corpus_first, corpus_last );
            }
            
    private:
        ForwardIterator first_;
        ForwardIterator last_;
        Hash hash_;					//	do I need this?
        BinaryPredicate pred_;		//	I'm pretty sure I need this
        const difference_type k_pattern_length;
        std::unordered_map<value_type, difference_type, Hash, BinaryPredicate> skip_;
        std::vector <difference_type> suffix_;

        /// \fn operator ( corpusIter corpus_first, corpusIter corpus_last, Pred p )
        /// \brief Searches the corpus for the pattern that was passed into the constructor
        /// 
        /// \param corpus_first The start of the data to search (Random Access Iterator)
        /// \param corpus_last  One past the end of the data to search
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
                while ( pred_ ( first_ [j-1], curPos [j-1] )) {
                    j--;
                //  We matched - we're done!
                    if ( j == 0 )
                        return curPos;
                    }
                
            //  Since we didn't match, figure out how far to skip forward
         	//	k = skip_ [ ];
	            auto it = skip_.find ( curPos [ j - 1 ] );
                k = it == skip_.end () ? k_pattern_length : it->second;
         	
                m = j - k - 1;
                if ( k < j && m > suffix_ [ j ] )
                    curPos += m;
                else
                    curPos += suffix_ [ j ];
                }
        
            return corpus_last;     // We didn't find anything
            }


        void build_skip_table ( ForwardIterator first, ForwardIterator last ) {
            for ( std::size_t i = 0; first != last; ++first, ++i )
            	skip_ [ *first ] = i;
          //    skip_.insert ( *first, i );
            }
        

        template<typename Iter, typename Container>
        void compute_bm_prefix ( Iter first, Iter last, BinaryPredicate pred, Container &prefix ) {
            const std::size_t count = std::distance ( first, last );
            assert ( count > 0 );
            assert ( prefix.size () == count );
                            
            prefix[0] = 0;
            std::size_t k = 0;
            for ( std::size_t i = 1; i < count; ++i ) {
                assert ( k < count );
                while ( k > 0 && !pred ( first[k], first[i] )) {
                    assert ( k < count );
                    k = prefix [ k - 1 ];
                    }
                    
                if ( pred ( first[k], first[i] ))
                    k++;
                prefix [ i ] = k;
                }
            }

        void build_suffix_table ( ForwardIterator first, ForwardIterator last, BinaryPredicate pred ) {
            const std::size_t count = (std::size_t) std::distance ( first, last );
            
            if ( count > 0 ) {  // empty pattern
                std::vector<typename std::iterator_traits<ForwardIterator>::value_type> reversed(count);
                (void) std::reverse_copy ( first, last, reversed.begin ());
                
                std::vector<difference_type> prefix (count);
                compute_bm_prefix ( first, last, pred, prefix );
        
                std::vector<difference_type> prefix_reversed (count);
                compute_bm_prefix ( reversed.begin (), reversed.end (), pred, prefix_reversed );
                
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

#if 0
    template <typename patIter, typename BinaryPredicate = typename std::equal_to<typename std::iterator_traits<patIter>::value_type>, typename traits = BM_traits<patIter, BinaryPredicate>>
    class boyer_moore_horspool_searcher {
        typedef typename std::iterator_traits<patIter>::difference_type difference_type;
    public:
        boyer_moore_horspool_searcher ( patIter first, patIter last, BinaryPredicate pred = BinaryPredicate ()) 
                : first_ ( first ), last_ ( last ), pred_ ( pred ),
                  k_pattern_length ( std::distance ( first_, last_ )),
                  skip_ ( k_pattern_length, k_pattern_length ) {
                  
            std::size_t i = 0;
            if ( first_ != last_ )    // empty pattern?
                for ( patIter iter = first_; iter != last_-1; ++iter, ++i )
                    skip_.insert ( *iter, k_pattern_length - 1 - i );
            }

        /// \fn operator ( corpusIter corpus_first, corpusIter corpus_last, Pred p )
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

            if ( corpus_first == corpus_last  ) return corpus_last;  // if nothing to search, we didn't find it!
            if (       first_ ==        last_ ) return corpus_first; // empty pattern matches at start

            const difference_type k_corpus_length  = std::distance ( corpus_first, corpus_last );
        //  If the pattern is larger than the corpus, we can't find it!
            if ( k_corpus_length < k_pattern_length )
                return corpus_last;
    
        //  Do the search 
            return this->do_search ( corpus_first, corpus_last );
            }

    private:
        patIter first_, last_;
        BinaryPredicate pred_;
        const difference_type k_pattern_length;
        typename traits::skip_table_t skip_;

        /// \fn do_search ( corpusIter corpus_first, corpusIter corpus_last )
        /// \brief Searches the corpus for the pattern that was passed into the constructor
        /// 
        /// \param corpus_first The start of the data to search (Random Access Iterator)
        /// \param corpus_last  One past the end of the data to search
        ///
        template <typename corpusIter>
        corpusIter do_search ( corpusIter corpus_first, corpusIter corpus_last ) const {
            corpusIter curPos = corpus_first;
            const corpusIter lastPos = corpus_last - k_pattern_length;
            while ( curPos <= lastPos ) {
            //  Do we match right where we are?
                std::size_t j = k_pattern_length - 1;
                while ( pred_ ( first_ [j], curPos [j] )) {
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
#endif

template <typename Iterator, typename BinaryPredicate = typename std::equal_to<typename std::iterator_traits<Iterator>::value_type>>
default_searcher<Iterator, BinaryPredicate> make_searcher ( Iterator first, Iterator last, BinaryPredicate pred = BinaryPredicate ()) {
	return default_searcher<Iterator, BinaryPredicate> ( first, last, pred );
	}

template <typename ForwardIterator, 
          typename Hash =            typename std::hash    <typename std::iterator_traits<ForwardIterator>::value_type>,
          typename BinaryPredicate = typename std::equal_to<typename std::iterator_traits<ForwardIterator>::value_type>>
boyer_moore_searcher<ForwardIterator, Hash, BinaryPredicate> make_boyer_moore_searcher ( 
	ForwardIterator first, ForwardIterator last, Hash hash = Hash (), BinaryPredicate pred = BinaryPredicate ()) {
	return boyer_moore_searcher<ForwardIterator, Hash, BinaryPredicate> ( first, last, hash, pred );
	}

#if 0
template <typename Iterator, typename BinaryPredicate = typename std::equal_to<typename std::iterator_traits<Iterator>::value_type>, typename traits=BM_traits<Iterator, BinaryPredicate>>
boyer_moore_horspool_searcher<Iterator, BinaryPredicate, traits> make_boyer_moore_horspool_searcher ( Iterator first, Iterator last, BinaryPredicate pred = BinaryPredicate ()) {
	return boyer_moore_horspool_searcher<Iterator, BinaryPredicate, traits> ( first, last, pred );
	}
#endif
}
