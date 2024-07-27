/*
 * Copyright © 2009-2010, 2012-2014 marmuta <marmvta@gmail.com>
 *
 * This file is part of Onboard.
 *
 * Onboard is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Onboard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef LM_H
#define LM_H

#include <stdint.h>
#include <stdio.h>
#include <optional>
#include <string.h>
#include <iconv.h>
#include <errno.h> // EINVAL
#include <wchar.h>
#include <vector>
#include <map>
#include <algorithm>
#include <string>

#include "lm_decls.h"
#include "pool_allocator.h"

class UString;

namespace lm {

using Token = std::wstring;
using Tokens = std::vector<Token>;
using TokenView = std::wstring_view;
using TokenViews = std::vector<TokenView>;

// break into debugger
// step twice to come back out of the raise() call into known code
#define BREAK raise(SIGINT)

//#undef LMDEBUG
#define ASSERT(c) assert(c)
//#ifdef LMDEBUG
//#define ASSERT(c) assert(c)
//#else
//#define ASSERT(c) /*c*/
//#endif

#ifndef ALEN
#define ALEN(a) ((int)(sizeof(a)/sizeof(*a)))
#endif

// WordId type
typedef uint32_t WordId;
//typedef uint16_t WordId;
#define WIDNONE ((WordId)-1)

typedef int OrderType;

// Number of sub-nodes type
//typedef uint16_t InplaceSize;
typedef uint32_t InplaceSize;

// count (ngram frequency) type
typedef uint32_t CountType;


enum ControlWords
{
    UNKNOWN_WORD_ID = 0,
    BEGIN_OF_SENTENCE_ID,
    END_OF_SENTENCE_ID,
    NUMBER_ID,
    NUM_CONTROL_WORDS
};

enum LMError
{
    ERR_NOT_IMPL = -1,
    ERR_NONE = 0,
    ERR_FILE,
    ERR_MEMORY,
    ERR_NUMTOKENS,
    ERR_ORDER_UNEXPECTED,
    ERR_ORDER_UNSUPPORTED,
    ERR_COUNT,
    ERR_UNEXPECTED_EOF,
    ERR_WC2MB,
    ERR_MD2WC,
};

class Exception : public std::runtime_error
{
    public:
        Exception(const std::string& msg, LMError error_) :
            std::runtime_error(msg),
            error(error_)
        {}
        LMError error{ERR_NONE};
};

std::string get_error_msg(const LMError err, const char* filename);
void throw_on_error(const LMError err, const char* filename = NULL);

template <class T>
int binsearch(const std::vector<T>& v, T key)
{
    typename std::vector<T>::const_iterator it = lower_bound(v.begin(), v.end(), key);
    if (it != v.end() && *it == key)
        return int(it - v.begin());
    return -1;
}

class StrConv
{
    public:
        StrConv();
        ~StrConv();

        // decode multi-byte to wide-char
        const wchar_t* mb2wc (const char* instr) const
        {
            char* inptr = const_cast<char*>(instr);
            size_t inbytes = strlen(instr);

            static char outstr[4096];
            char* outptr = outstr;
            size_t outbytes = sizeof(outstr);

            size_t nconv;

            nconv = iconv (cd_mb_wc, &inptr, &inbytes, &outptr, &outbytes);
            if (nconv == (size_t) -1)
            {
                // Not everything went right.  It might only be
                // an unfinished byte sequence at the end of the
                // buffer.  Or it is a real problem.
                if (errno != EINVAL)
                {
                    // It is a real problem.  Maybe we ran out of space
                    // in the output buffer or we have invalid input.
                    return NULL;
                }
            }

            // Terminate the output string.
            if (outbytes >= sizeof (wchar_t))
                *((wchar_t *) outptr) = L'\0';

            return (wchar_t *) outstr;
        }

        // encode wide-char to multi-byte
        const char* wc2mb (const wchar_t *instr)
        {
            char* inptr = (char*)instr;
            size_t inbytes = wcslen(instr) * sizeof(*instr);

            static char outstr[4096];
            char* outptr = outstr;
            size_t outbytes = sizeof(outstr);

            size_t nconv = iconv(cd_wc_mb, &inptr, &inbytes,
                                           &outptr, &outbytes);
            if (nconv == (size_t) -1)
            {
                // Not everything went right.  It might only be
                // an unfinished byte sequence at the end of the
                // buffer.  Or it is a real problem.
                if (errno != EINVAL)
                {
                    // It is a real problem.  Maybe we ran out of space
                    // in the output buffer or we have invalid input.
                    return NULL;
                }
            }

            // Terminate the output string.
            if (outbytes >= sizeof (wchar_t))
                *outptr = '\0';

            return outstr;
        }
    private:
        iconv_t cd_mb_wc;
        iconv_t cd_wc_mb;
};


//------------------------------------------------------------------------
// Dictionary - contains the vocabulary of the language model
//------------------------------------------------------------------------

class Dictionary
{
    public:
        Dictionary();

        void clear();

        void dump();

        // assumes utf-8 string
        // faster, no encoding needed
        WordId word_to_id(const char* word);

        // assumes utf-32 string, requires encoding
        WordId word_to_id(const wchar_t* word);

        const char* id_to_word_utf8(WordId wid) const;
        const wchar_t* id_to_word_w(WordId wid) const;

        std::vector<WordId> words_to_ids(const wchar_t** word, int n);

        void ids_to_words(std::vector<std::string>& m_words,
                          const std::vector<WordId>& wids);
        void ids_to_words(std::vector<const char*>& m_words,
                          const std::vector<WordId>& wids);

        LMError set_words(const std::vector<const char*>& new_words);
        WordId add_word(const char* word);  // utf-8
        WordId add_word(const wchar_t* word);

        WordId query_add_word(const char* word, bool allow_new_words)
        {
            WordId wid = word_to_id(word);
            if (wid == WIDNONE)
            {
                if (allow_new_words)
                    return add_word(word);
                else
                    return UNKNOWN_WORD_ID;
            }
            return wid;
        }

        WordId query_add_word(const wchar_t* word, bool allow_new_words)
        {
            WordId wid = word_to_id(word);
            if (wid == WIDNONE)
            {
                if (allow_new_words)
                    return add_word(word);
                else
                    return UNKNOWN_WORD_ID;
            }
            return wid;
        }

        // get word ids, add unknown words as needed
        bool query_add_words(std::vector<WordId>& wids,
                             const std::vector<std::string>& new_words,
                             bool allow_new_words = true)
        {
            for (size_t i = 0; i < new_words.size(); i++)
            {
                WordId wid = query_add_word(new_words[i].c_str(), allow_new_words);
                if (wid == WIDNONE)
                    return false;
                wids[i] = wid;
            }
            return true;
        }

        bool query_add_words(std::vector<WordId>& wids,
                             const std::vector<const char*>& new_words,
                             bool allow_new_words = true)
        {
            for (size_t i = 0; i < new_words.size(); i++)
            {
                WordId wid = query_add_word(new_words[i], allow_new_words);
                if (wid == WIDNONE)
                    return false;
                wids[i] = wid;
            }
            return true;
        }
        bool query_add_words(std::vector<WordId>& wids,
                             const char* const* new_words, size_t n,
                             bool allow_new_words = true)
        {
            for (size_t i = 0; i < n; i++)
            {
                WordId wid = query_add_word(new_words[i], allow_new_words);
                if (wid == WIDNONE)
                    return false;
                wids[i] = wid;
            }
            return true;
        }

        bool query_add_words(std::vector<WordId>& wids,
                             const std::vector<const wchar_t*> new_words,
                             bool allow_new_words = true)
        {
            for (size_t i = 0; i < new_words.size(); i++)
            {
                WordId wid = query_add_word(new_words[i], allow_new_words);
                if (wid == WIDNONE)
                    return false;
                wids[i] = wid;
            }
            return true;
        }

        bool contains(const wchar_t* word) {return word_to_id(word) != WIDNONE;}

        void prefix_search(const wchar_t* prefix,
                           std::vector<WordId>* wids_in,  // may be NULL
                           std::vector<WordId>& wids_out,
                           uint32_t options = 0);
        int lookup_word(const wchar_t* word);

        int get_num_word_types() {return m_words.size();}

        uint64_t get_memory_size();

    protected:
        int search_index(const char* word)
        {
            int index;
            if (sorted)
                index = binsearch_sorted(word);
            else
            {
                // search non-control words
                index = binsearch_words(word);

                // else try to find a control word match
                if (index >= (int)m_words.size() ||
                    strcmp(m_words[index], word) != 0)
                {
                    for (int i=0; i<sorted_words_begin; i++)
                        if (strcmp(m_words[i], word) == 0)
                        {
                            index = i;
                            break;
                        }
                }
            }
            return index;
        }

        // binary search for index of insertion point (std:lower_bound())
        int binsearch_sorted(const char* word)
        {
            int lo = 0;
            int hi = sorted->size();
            while (lo < hi)
            {
                int mid = (lo+hi)>>1;
                int cmp = strcmp(m_words[(*sorted)[mid]], word);
                if (cmp < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        // binary search for index of insertion point (std:lower_bound())
        int binsearch_words(const char* word)
        {
            int lo = sorted_words_begin;
            int hi = m_words.size();
            while (lo < hi)
            {
                int mid = (lo+hi)>>1;
                int cmp = strcmp(m_words[mid], word);
                if (cmp < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        void update_sorting(const char* word, WordId wid);

    protected:
        std::vector<char*> m_words;
        std::vector<WordId>* sorted;  // only when words aren't already sorted
        int sorted_words_begin;
        StrConv conv;
};


//------------------------------------------------------------------------
// LanguageModel - base class of language models
//------------------------------------------------------------------------

class LanguageModel
{
    public:

    public:
        LanguageModel();

        virtual ~LanguageModel();

        virtual void clear()
        {
            m_dictionary.clear();
        }

        // never fails
        virtual WordId word_to_id(const wchar_t* word)
        {
            WordId wid = m_dictionary.word_to_id(word);
            if (wid == WIDNONE)
                return UNKNOWN_WORD_ID;   // map to always existing <unk> entry
            return wid;
        }

        std::vector<WordId> words_to_ids(const std::vector<const wchar_t*>& words)
        {
            std::vector<WordId> wids;
            std::vector<const wchar_t*>::const_iterator it;
            for(it=words.begin(); it!=words.end(); it++)
                wids.push_back(word_to_id(*it));
            return wids;
        }

        // never fails
        const wchar_t* id_to_word(WordId wid)
        {
            static const wchar_t* not_found = L"";
            const wchar_t* w = m_dictionary.id_to_word_w(wid);
            if (!w)
                return not_found;
            return w;
        }

        int lookup_word(const UString& word);
        int lookup_word(const wchar_t* word)
        {
            return m_dictionary.lookup_word(word);
        }

        virtual void predict(std::vector<UString>& uresults,
                             const std::vector<UString>& ucontext,
                             std::optional<size_t> limit={},
                             PredictOptions options = DEFAULT_OPTIONS);

        virtual void predict(std::vector<UPredictResult>& uresults,
                             const std::vector<UString>& ucontext,
                             std::optional<size_t> limit={},
                             PredictOptions options = DEFAULT_OPTIONS);

        virtual void predict(std::vector<PredictResult>& results,
                             const std::vector<const wchar_t*>& context,
                             int limit=-1,
                             PredictOptions options = DEFAULT_OPTIONS);

        virtual double get_probability(const wchar_t* const* ngram, int n);

        virtual int get_num_word_types() {return m_dictionary.get_num_word_types();}

        virtual bool is_model_valid() = 0;

        // throw exceptions, record errors
        void load(const std::string& filename) {load(filename.c_str());}
        void save(const std::string& filename) {save(filename.c_str());}
        virtual void load(const char* filename) = 0;
        virtual void save(const char* filename) = 0;

        // don't throw exceptions, low level
        virtual LMError do_load(const char* filename) = 0;
        virtual LMError do_save(const char* filename) = 0;

        virtual LMError get_load_error() = 0;
        virtual std::string get_load_error_msg() = 0;
        virtual void set_load_error_msg(const std::string& msg) = 0;

        virtual bool is_modified() = 0;
        virtual void set_modified(bool modified) = 0;

    protected:
        const wchar_t* split_context(const std::vector<const wchar_t*>& context,
                                     std::vector<const wchar_t*>& history);
        virtual void get_words_with_predictions(
                                     const std::vector<WordId>& history,
                                     std::vector<WordId>& wids)
        {
            (void)history;
            (void)wids;
        }
        virtual void get_candidates(const std::vector<WordId>& history,
                                    const wchar_t* prefix,
                                    std::vector<WordId>& wids,
                                    uint32_t options);
        virtual void filter_candidates(const std::vector<WordId>& in,
                                             std::vector<WordId>& out)
        {
            std::copy(in.begin(), in.end(), std::back_inserter(out));
        }
        virtual void get_probs(const std::vector<WordId>& history,
                               const std::vector<WordId>& words,
                               std::vector<double>& probabilities)
        {
            (void)history;
            (void)words;
            (void)probabilities;
        }
        LMError read_utf8(const char* filename, wchar_t*& text);

    public:
        Dictionary m_dictionary;
};


//------------------------------------------------------------------------
// NGramModel - base class of n-gram language models, may go away
//------------------------------------------------------------------------

class NGramModel : public LanguageModel
{
    public:
        NGramModel(OrderType order=0) :
            m_order(order)
        {
        }

        virtual int get_order()
        {
            return this->m_order;
        }

        virtual void set_order(int n)
        {
            this->m_order = n;
            clear();
        }

        virtual int get_max_order()
        {
            return 0;  // 0: unlimited
        }

        #ifdef LMDEBUG
        void print_ngram(const std::vector<WordId>& wids);
        #endif

    public:
        int m_order;
};

void to_string(std::vector<std::string>& vs,
               const std::vector<const char*>& vc);
void to_string(std::vector<std::string>& vs,
               const std::vector<UString>& vu);
void to_wchar(std::vector<const wchar_t*>& vc,
              const std::vector<std::wstring>& vw);
void to_wstring(std::vector<std::wstring>& vw,
                const std::vector<UString>& vu);
void to_wstring(std::vector<std::wstring>& vw,
                const std::vector<const wchar_t*>& vc);
void to_wstring(std::vector<std::wstring>& vw,
                const wchar_t** vc, int n);
void to_ustring(std::vector<UString>& vu,
                const std::vector<const char*>& vc);
void to_ustring(std::vector<UString>& vu,
                const std::vector<const wchar_t*>& vc);

// Read corpus, encoding may be 'utf-8', 'latin-1'.
void read_corpus(std::wstring& result,
                 const std::string& filename,
                 const std::string& encoding={},
                 size_t num_lines={});

LMError do_read_corpus(std::wstring& result,
                 const std::string& filename,
                 const std::string& encoding={},
                 size_t num_lines={});

// Read the order from the header of the given arpac-like file.
// Encoding may be 'utf-8', 'latin-1'.
std::optional<OrderType> read_order(const std::string& filename,
                                    const std::string& encoding={});

}  // namespace

#endif

