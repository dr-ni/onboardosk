/*
 * Copyright © 2013-2014 marmuta <marmvta@gmail.com>
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

#ifndef LM_UNIGRAM_H
#define LM_UNIGRAM_H

#include "lm_dynamic.h"

namespace lm {

//------------------------------------------------------------------------
// UnigramModel - Memory efficient model for word frequencies.
//------------------------------------------------------------------------
class UnigramModel : public DynamicModelBase
{
    public:
        class ngrams_iter : public DynamicModelBase::ngrams_iter
        {
            public:
                ngrams_iter(const UnigramModel* lm) :
                    it(lm->m_counts.begin()),
                    model(lm)
                {}

                virtual BaseNode* operator*() const // dereference operator
                {
                    if (it == model->m_counts.end())
                        return NULL;
                    else
                    {
                        BaseNode* pnode = const_cast<BaseNode*>(&node);
                        pnode->m_count = *it;
                        return const_cast<BaseNode*>(&node);
                    }
                }

                virtual void operator++(int unused)
                {
                    (void)unused;
                    it++;
                }

                virtual void get_ngram(std::vector<WordId>& ngram)
                {
                    WordId wid = it - model->m_counts.begin();
                    ngram.resize(1);
                    ngram[0] = wid;
                }

                virtual int get_level()
                { return 1; }

                virtual bool at_root()
                { return false; }

            public:
                std::vector<CountType>::const_iterator it;
                const UnigramModel* model;
                BaseNode node;  // dummy node to satisfy the NGramIter interface
        };
        virtual std::unique_ptr<DynamicModelBase::ngrams_iter> ngrams_begin() const override
        {return std::make_unique<ngrams_iter>(this);}

    public:
        UnigramModel()
        {
            set_order(1);
        }

        virtual ~UnigramModel()
        {
        }

        virtual std::unique_ptr<DynamicModelBase> clone_empty() override
        {
            std::unique_ptr<UnigramModel> model = std::make_unique<UnigramModel>();
            return model;
        }

        virtual void clear() override
        {
            std::vector<CountType>().swap(m_counts); // clear and really free the memory
            DynamicModelBase::clear();  // clears dictionary
        }

        virtual int get_max_order() override
        {
            return 1;
        }

        virtual Smoothing get_smoothing() override {return Smoothing::SMOOTHING_NONE;}
        virtual void set_smoothing(Smoothing s) override {(void) s;}

        virtual BaseNode* count_ngram(const std::vector<std::string>& ngram,
                                      int increment, bool allow_new_words) override
        {
            size_t n = ngram.size();
            if (n == 1)
            {
                std::vector<WordId> wids(n);
                if (m_dictionary.query_add_words(wids, ngram, allow_new_words))
                    return count_ngram(&wids[0], n, increment);
            }
            return NULL;
        }
        virtual BaseNode* count_ngram(const std::vector<const char*>& ngram,
                                      int increment, bool allow_new_words) override
        {
            size_t n = ngram.size();
            if (n == 1)
            {
                std::vector<WordId> wids(n);
                if (m_dictionary.query_add_words(wids, ngram, allow_new_words))
                    return count_ngram(&wids[0], n, increment);
            }
            return NULL;
        }
        virtual BaseNode* count_ngram(const char* const* ngram, size_t n,
                                      int increment, bool allow_new_words=true) override
        {
            if (n == 1)
            {
                std::vector<WordId> wids(n);
                if (m_dictionary.query_add_words(wids, ngram, n, allow_new_words))
                    return count_ngram(&wids[0], n, increment);
            }
            return NULL;
        }
        virtual BaseNode* count_ngram(std::vector<const wchar_t*> ngram,
                                      int increment=1, bool allow_new_words=true) override
        {
            size_t n = ngram.size();
            if (n == 1)
            {
                std::vector<WordId> wids(n);
                if (m_dictionary.query_add_words(wids, ngram, allow_new_words))
                    return count_ngram(&wids[0], n, increment);
            }
            return NULL;
        }

        virtual BaseNode* count_ngram(const WordId* wids, int n, int increment) override
        {
            if (n != 1)
                return NULL;

            WordId wid = wids[0];
            if (m_counts.size() <= wid)
                m_counts.push_back(0);

            m_counts.at(wid) += increment;

            m_node.m_word_id = wid;
            m_node.m_count = m_counts[wid];
            return &m_node;
        }

        virtual int get_ngram_count(const char* const* ngram, int n) override
        {
            if (!n)
                return 0;
            WordId wid = m_dictionary.word_to_id(ngram[0]);
            return /*wid >= 0 &&*/ wid < m_counts.size() ? m_counts.at(wid) : 0;
        }
        virtual int get_ngram_count(const wchar_t* const* ngram, int n) override
        {
            if (!n)
                return 0;
            WordId wid = m_dictionary.word_to_id(ngram[0]);
            return /*wid >= 0 &&*/ wid < m_counts.size() ? m_counts.at(wid) : 0;
        }

        virtual void get_node_values(const BaseNode* node, int level,
                                     std::vector<int>& values) const override
        {
            (void)level;
            values.push_back(node->m_count);
        }

        virtual bool is_model_valid() override
        {
            int num_unigrams = get_num_ngrams(0);
            return num_unigrams == m_dictionary.get_num_word_types();
        }

        virtual void get_memory_sizes(std::vector<long>& values)
        {
            values.push_back(m_dictionary.get_memory_size());
            values.push_back(sizeof(CountType) * m_counts.capacity());
        }

    protected:
        virtual void get_words_with_predictions(
                                       const std::vector<WordId>& history,
                                       std::vector<WordId>& wids)
        {
            (void)history;
            (void)wids;
        }
        virtual void get_probs(const std::vector<WordId>& history,
                               const std::vector<WordId>& words,
                               std::vector<double>& probabilities);

        virtual int get_num_ngrams(int level)
        {
            if (level == 0)
                return m_counts.size();
            else
                return 0;
        }

        virtual void reserve_unigrams(int count)
        {
            m_counts.resize(count);
            fill(m_counts.begin(), m_counts.end(), 0);
        }

    protected:
        std::vector<CountType> m_counts;
        BaseNode m_node;  // dummy node to satisfy the count_ngram interface
};

}  // namespace

#endif

