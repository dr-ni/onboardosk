/*
 * Copyright © 2009-2010, 2013-2014 marmuta <marmvta@gmail.com>
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

#include <error.h>

#include "tools/ustringmain.h"

#include "lm_dynamic.h"

namespace lm {

//------------------------------------------------------------------------
// DynamicModelBase
//------------------------------------------------------------------------

// Load from ARPA-like format, expects counts instead of log probabilities
// and no back-off values. N-grams don't have to be sorted alphabetically.
// State machine driven version, still the fastest.
LMError DynamicModelBase::load_arpac(const char* filename)
{
    int i;
    int new_order = 0;
    int current_level = 0;
    int line_number = -1;
    std::vector<int> counts;
    LMError err_code = ERR_NONE;

    enum {BEGIN, COUNTS, NGRAMS_HEAD, NGRAMS, DONE}
    state = BEGIN;

    std::vector<Unigram> unigrams;

    clear();

    FILE* f = fopen(filename, "r");
    if (!f)
    {
        #ifdef LMDEBUG
        printf( "Error opening %s\n", filename);
        #endif
        return ERR_FILE;
    }

    while(1)
    {
        // read line
        char buf[4096];
        if (fgets(buf, ALEN(buf), f) == NULL)
            break;
        line_number++;

        // chop line into tokens
        char *tstate;
        char* tokens[32] = {strtok_r(buf, " \n", &tstate)};
        for (i=0; tokens[i] && i < ALEN(tokens)-1; i++)
            tokens[i+1] = strtok_r(NULL, " \n", &tstate);
        int ntoks = i;

        if (ntoks)  // any tokens there?
        {
            // check for n-grams first, this is by far the most frequent case
            if (state == NGRAMS)
            {
                if (tokens[0][0] == '\\')  // end of section?
                {
                    // add unigrams
                    if (current_level == 1)
                    {
                        err_code = set_unigrams(unigrams);
                        std::vector<Unigram>().swap(unigrams); // really free mem
                        if (err_code)
                            break;
                    }

                    // check count
                    int ngrams_expected = counts[current_level-1];
                    int ngrams_read = get_num_ngrams(current_level-1);
                    if (ngrams_read != ngrams_expected)
                    {
                        error (0, 0, "unexpected n-gram count for level %d: "
                                     "expected %d n-grams, but read %d",
                              current_level,
                              ngrams_expected, ngrams_read);
                        err_code = ERR_COUNT; // count doesn't match number of unique ngrams
                        break;
                    }
                    state = NGRAMS_HEAD;
                }
                else
                {
                    if (ntoks < current_level+1)
                    {
                        err_code = ERR_NUMTOKENS; // too few tokens for cur. level
                        error (0, 0, "too few tokens for n-gram level %d: "
                              "line %d, tokens found %d/%d",
                              current_level,
                              line_number, ntoks, current_level+1);
                        break;
                    }

                    int itok = 0;
                    int count = strtol(tokens[itok++], NULL, 10);

                    uint32_t time = 0;
                    if (ntoks >= current_level+2)
                        time  = strtol(tokens[itok++], NULL, 10);

                    // There is a slight possibility that old models have
                    // zero counts. Since rev. 1845 n-grams with zero count
                    // are considered removed, which causes load failures.
                    // -> ignore n-grams with count 0
                    if (count <= 0)
                    {
                        // Expect one n-gram fewer for this level.
                        counts[current_level-1]--;
                    }
                    else
                    {
                        if (current_level == 1)
                        {
                            // Temporarily collect unigrams so we can sort them.
                            Unigram unigram = {tokens[itok],
                                               (CountType)count,
                                               time};
                            unigrams.push_back(unigram);
                        }
                        else
                        {
                            BaseNode* node = count_ngram(tokens+itok,
                                                         current_level,
                                                         count);
                            if (!node)
                            {
                                err_code = ERR_MEMORY; // out of memory
                                break;
                            }
                            set_node_time(node, time);
                        }
                    }

                    continue;
                }
            }
            else
            if (state == BEGIN)
            {
                if (strncmp(tokens[0], "\\data\\", 6) == 0)
                {
                    state = COUNTS;
                }
            }
            else
            if (state == COUNTS)
            {
                if (strncmp(tokens[0], "ngram", 5) == 0 && ntoks >= 2)
                {
                    int level;
                    int count;
                    if (sscanf(tokens[1], "%d=%d", &level, &count) == 2)
                    {
                        new_order = std::max(new_order, level);
                        counts.resize(new_order);
                        counts[level-1] = count;
                    }
                }
                else
                {
                    int max_order = get_max_order();
                    if (max_order && max_order < new_order)
                    {
                        err_code = ERR_ORDER_UNSUPPORTED;
                        break;
                    }

                    // clear language model and set it up for the new order
                    set_order(new_order);
                    if (new_order)
                    {
                        // This drops control words! They are added back
                        // with assure_valid_control_words() below.
                        reserve_unigrams(counts[0]);
                    }
                    state = NGRAMS_HEAD;
                }
            }

            if (state == NGRAMS_HEAD)
            {
                if (sscanf(tokens[0], "\\%d-grams", &current_level) == 1)
                {
                    if (current_level < 1 || current_level > new_order)
                    {
                        err_code = ERR_ORDER_UNEXPECTED;
                        break;
                    }
                    state = NGRAMS;
                }
                else
                if (strncmp(tokens[0], "\\end\\", 5) == 0)
                {
                    state = DONE;
                    break;
                }
            }
        }
    }

    // didn't make it until the end?
    if (state != DONE)
    {
        clear();
        if (!err_code)
            err_code = ERR_UNEXPECTED_EOF;  // unexpected end of file
    }

    // At this point, control words might possibly have been loaded with
    // zero counts. Make sure they exist with at least count 1.
    assure_valid_control_words();

    return err_code;
}


// Save to ARPA-like format, stores counts instead of log probabilities
// and no back-off values.
LMError DynamicModelBase::save_arpac(const char* filename)
{
    int i;

    FILE* f = fopen(filename, "w,ccs=UTF-8");
    if (!f)
    {
        #ifdef LMDEBUG
        printf( "Error opening %s", filename);
        #endif
        return ERR_FILE;
    }

    fwprintf(f, L"\n");
    fwprintf(f, L"\\data\\\n");

    for (i=0; i<this->m_order; i++)
        fwprintf(f, L"ngram %d=%d\n", i+1, get_num_ngrams(i));

    write_arpa_ngrams(f);

    fwprintf(f, L"\n");
    fwprintf(f, L"\\end\\\n");

    fclose(f);

    return ERR_NONE;
}

LMError DynamicModelBase::write_arpa_ngrams(FILE* f)
{
    int i;

    for (i=0; i<this->m_order; i++)
    {
        fwprintf(f, L"\n");
        fwprintf(f, L"\\%d-grams:\n", i+1);

        std::vector<WordId> wids;
        for (auto it = ngrams_begin(); ; (*it)++)
        {
            const BaseNode* node = *(*it);
            if (!node)
                break;

            if (it->get_level() == i+1)
            {
                it->get_ngram(wids);
                LMError error = write_arpa_ngram(f, node, wids);
                if (error)
                    return error;
            }
        }
    }

    return ERR_NONE;
}

// add unigrams in bulk
LMError DynamicModelBase::set_unigrams(const std::vector<Unigram>& unigrams)
{
    LMError error = ERR_NONE;

    // Add all words in bulk to the dictionary.
    // -> they are stored sorted and don't need
    // the sorted array -> saves memory.
    std::vector<const char*> words;
    words.reserve(unigrams.size());
    std::vector<Unigram>::const_iterator it;
    for (it=unigrams.begin(); it != unigrams.end(); it++)
    {
        const Unigram& unigram = *it;
        words.push_back(unigram.word.c_str());
    }
    error = m_dictionary.set_words(words);

    if (!error)
    {
        // finally add all the unigrams
        for (it=unigrams.begin(); it < unigrams.end(); it++)
        {
            const Unigram& unigram = *it;
            const char* word = unigram.word.c_str();
            BaseNode* node = count_ngram(&word,
                                         1,
                                         unigram.count);
            if (!node)
            {
                error = ERR_MEMORY; // out of memory
                break;
            }
            set_node_time(node, unigram.time);
        }
    }
    return error;
}

void DynamicModelBase::assure_valid_control_words()
{
    const char* words[NUM_CONTROL_WORDS] =
    {"<unk>", "<s>", "</s>", "<num>"};

    for (WordId i=0; i<ALEN(words); i++)
    {
        if (get_ngram_count(words+i, 1) <= 0)
            count_ngram(words+i, 1, 1);

        // Control words must have fixed positions at
        // the very beginning of the dictionary.
        assert(m_dictionary.word_to_id(words[i]) == i);
    }
}

void DynamicModelBase::copy_contents(const DynamicModelBase* model)
{
    std::vector<WordId> wids;
    std::vector<const char*> words;
    for (auto it = model->ngrams_begin(); ; (*it)++)
    {
        const BaseNode* node = *(*it);
        if (!node)
            break;

        it->get_ngram(wids);
        int level = static_cast<int>(wids.size());

        words.clear();
        for (size_t i=0; i<wids.size(); i++)
            words.emplace_back(model->m_dictionary.id_to_word_utf8(wids[i]));

        std::vector<int> values;
        model->get_node_values(node, level, values);

        count_ngram(words, values[0]);
    }
}

void DynamicModelBase::learn_tokens(const std::vector<UString>& utokens,
                                    bool allow_new_words)
{
    std::vector<std::string> tokens;
    to_string(tokens, utokens);
    learn_tokens(tokens, allow_new_words);
}


void DynamicModelBase::load(const char* filename)
{
    m_load_error_msg = "";
    m_modified = false;
    m_load_error = do_load(filename);
    if (m_load_error)
    {
        m_load_error_msg = get_error_msg(m_load_error, filename);
        throw_on_error(m_load_error, filename);
    }
}

void DynamicModelBase::save(const char* filename)
{
    LMError error = do_save(filename);
    throw_on_error(error, filename);

    // Don't reset modified state, let the caller do it.
    // We might be saving to a temp file and renaming can still fail.
    // m_modified = false;
}

void DynamicModelBase::dump()
{
    m_dictionary.dump();

    std::vector<WordId> wids;
    for (auto it = ngrams_begin(); ; (*it)++)
    {
        const BaseNode* node = *(*it);
        if (!node)
            break;

        it->get_ngram(wids);

        std::vector<int> values;
        get_node_values(node, wids.size(), values);

        unsigned i;
        for (i=0; i<wids.size(); i++)
            printf("%ls ", m_dictionary.id_to_word_w(wids[i]));
        for (i=0; i<values.size(); i++)
            printf("%d ", values[i]);
        printf("\n");
    }
    printf("\n");
}

std::unique_ptr<DynamicModelBase> DynamicModelBase::prune(const std::vector<int>& prune_counts)
{
    // drop order for to be emptied n-gram levels
    int new_order = m_order;
    for (auto it=prune_counts.rbegin(); it != prune_counts.rend(); it++)
    {
        int prune_count = *it;
        if (prune_count != -1)
            break;
        new_order -= 1;
    }

    new_order = std::max(new_order, 2);
    std::unique_ptr<DynamicModelBase> model = clone_empty();
    model->set_order(new_order);

    std::vector<WordId> wids;
    std::vector<const char*> words;
    std::vector<int> values;

    for (auto it = ngrams_begin(); ; (*it)++)
    {
        const BaseNode* node = *(*it);
        if (!node)
            break;

        it->get_ngram(wids);
        int level = static_cast<int>(wids.size());

        values.clear();
        model->get_node_values(node, level, values);
        int count = values.at(0);

        int k = std::min(static_cast<int>(prune_counts.size()),
                         level) - 1;
        int prune_count = prune_counts.at(static_cast<size_t>(k));

        if (count > prune_count &&  prune_count != -1)
        {
            words.clear();
            m_dictionary.ids_to_words(words, wids);

            model->count_ngram(words, count);
        }
    }

    return model;
}

}
