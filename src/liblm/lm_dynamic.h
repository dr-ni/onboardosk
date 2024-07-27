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

#ifndef LM_DYNAMIC_H
#define LM_DYNAMIC_H

#include <math.h>
#include <assert.h>
#include <cstring>   // memcpy
#include <memory>
#include <string>

#include "lm.h"
#include "lm_tokenize.h"

#define HONOR_REMOVED_NODES true

namespace lm {

using NGramContent = std::tuple<std::vector<std::string>, std::vector<int>>;
using NGramContents = std::vector<NGramContent>;

#pragma pack(2)

//------------------------------------------------------------------------
// inplace_vector - expects its elements in anonymous memory right after itself
//------------------------------------------------------------------------

template <class T>
class inplace_vector
{
    public:
        inplace_vector()
        {
            num_items = 0;
        }

        int capacity() const
        {
            return capacity(num_items);
        }

        static int capacity(int n)
        {
            if (n == 0)
                n = 1;

            // growth factor, lower for slower growth and less wasted memory
            // g=2.0: quadratic growth, double capacity per step
            // [int(1.25**math.ceil(math.log(x)/math.log(1.25))) for x in range (1,100)]
            double g = 1.25;
            return (int) pow(g,ceil(log(n)/log(g)));
        }

        int size() const
        {
            return num_items;
        }

        T* buffer()
        {
            return (T*) (((uint8_t*)(this) + sizeof(inplace_vector<T>)));
        }
        const T* buffer() const
        {
            return const_cast<inplace_vector<T>*>(this)->buffer();
        }

        T& operator [](int index)
        {
            ASSERT(index >= 0 && index <= capacity());
            return buffer()[index];
        }
        const T& operator [](int index) const
        {
            ASSERT(index >= 0 && index <= capacity());
            return buffer()[index];
        }

        T& back()
        {
            ASSERT(size() > 0);
            return buffer()[size()-1];
        }

        void push_back(T& item)
        {
            buffer()[size()] = item;
            num_items++;
            ASSERT(size() <= capacity());
        }

        void insert(int index, T& item)
        {
            T* p = buffer();
            for (int i=size()-1; i>=index; --i)
                p[i+1] = p[i];
            p[index] = item;
            num_items++;
            ASSERT(size() <= capacity());
        }

    public:
        InplaceSize num_items;
};


//------------------------------------------------------------------------
// BaseNode - base class of all trie nodes
//------------------------------------------------------------------------

class BaseNode
{
    public:
        BaseNode(WordId wid = -1)
        {
            m_word_id = wid;
            m_count = 0;
        }

        void clear()
        {
            m_count = 0;
        }

        int get_count() const
        {
            return m_count;
        }

        void set_count(int c)
        {
            m_count = c;
        }


    public:
        WordId m_word_id;
        CountType m_count;
};

//------------------------------------------------------------------------
// LastNode - leaf node of the ngram trie, trigram for order 3
//------------------------------------------------------------------------
template <class TBASE>
class LastNode : public TBASE
{
    public:
        LastNode(WordId wid = (WordId)-1)
        : TBASE(wid)
        {
        }
};

//------------------------------------------------------------------------
// BeforeLastNode - second to last node of the ngram trie, bigram for order 3
//------------------------------------------------------------------------
template <class TBASE, class TLASTNODE>
class BeforeLastNode : public TBASE
{
    public:
        BeforeLastNode(WordId wid = (WordId)-1)
        : TBASE(wid)
        {
        }

        TLASTNODE* add_child(WordId wid)
        {
            TLASTNODE node(wid);
            if (m_children.size())
            {
                int index = search_index(wid);
                m_children.insert(index, node);
                //printf("insert: index=%d wid=%d\n",index, wid);
                return &m_children[index];
            }
            else
            {
                m_children.push_back(node);
                //printf("push_back: size=%d wid=%d\n",(int)children.size(), wid);
                return &m_children.back();
            }
        }

        BaseNode* get_child(WordId wid)
        {
            if (m_children.size())
            {
                int index = search_index(wid);
                if (index < (int)m_children.size())
                    if (m_children[index].m_word_id == wid)
                        return &m_children[index];
            }
            return NULL;
        }

        BaseNode* get_child_at(int index)
        {
            return &m_children[index];
        }

        int search_index(WordId wid) const
        {
            int lo = 0;
            int hi = m_children.size();
            while (lo < hi)
            {
                int mid = (lo+hi)>>1;
                if (m_children[mid].m_word_id < wid)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        int get_N1prx() const
        {
            // Take removed nodes into account (count==0)
            int n = 0;
            if (HONOR_REMOVED_NODES)  // any removed nodes in the model?
            {
                for (int i=0; i<m_children.size(); i++)
                    if (m_children[i].get_count() > 0)
                        n++;
            }
            else
            {
                n = m_children.size();  // assumes all have counts>=1
            }

            return n;
        }

        int sum_child_counts() const
        {
            int sum = 0;
            for (int i=0; i<m_children.size(); i++)
                sum += m_children[i].get_count();
            return sum;
        }
    public:
        inplace_vector<TLASTNODE> m_children;  // has to be last
};

//------------------------------------------------------------------------
// TrieNode - node for all lower levels of the ngram trie, unigrams for order 3
//------------------------------------------------------------------------
template <class TBASE>
class TrieNode : public TBASE
{
    public:
        TrieNode(WordId wid = (WordId)-1)
        : TBASE(wid)
        {
        }

        void add_child(BaseNode* node)
        {
            if (m_children.size())
            {
                int index = search_index(node->m_word_id);
                m_children.insert(m_children.begin()+index, node);
                //printf("insert: index=%d wid=%d\n",index, wid);
            }
            else
            {
                m_children.push_back(node);
                //printf("push_back: size=%d wid=%d\n",(int)children.size(), wid);
            }
        }

        BaseNode* get_child(WordId wid, int& index)
        {
            if (m_children.size())
            {
                index = search_index(wid);
                if (index < (int)m_children.size())
                    if (m_children[index]->m_word_id == wid)
                        return m_children[index];
            }
            return NULL;
        }

        BaseNode* get_child_at(int index)
        {
            return m_children[index];
        }

        int search_index(WordId wid)
        {
            // binary search like lower_bound()
            int lo = 0;
            int hi = m_children.size();
            while (lo < hi)
            {
                int mid = (lo+hi)>>1;
                if (m_children[mid]->m_word_id < wid)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        int get_N1prx() const
        {
            int n = 0;
            if (HONOR_REMOVED_NODES)  // any removed nodes in the model?
            {
                for (int i=0; i<(int)m_children.size(); i++)
                    if (m_children[i]->get_count() > 0)
                        n++;
            }
            else
            {
                n = m_children.size();  // assumes all children have counts > 0

                // Unigrams <unk>, <s>,... may be empty initially. Don't count them
                // or predictions for small models won't sum close to 1.0
                for (int i=0; i<n && i<NUM_CONTROL_WORDS; i++)
                    if (m_children[0]->get_count() == 0)
                        n--;
            }
            return n;
        }

        int sum_child_counts() const
        {
            int sum = 0;
            for (const auto& child : m_children)
                sum += child->get_count();
            return sum;
        }
    public:
        std::vector<BaseNode*> m_children;
};

//------------------------------------------------------------------------
// NGramTrie - root node of the ngram trie
//------------------------------------------------------------------------
template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
class NGramTrie : public TNODE
{
    public:
        class iterator
        {
            public:
                iterator()
                {
                }

                iterator(const NGramTrie* root)
                {
                    m_root = root;
                    m_nodes.push_back(root);
                    m_indexes.push_back(0);
                    operator++(0);
                }

                const BaseNode* operator*() const // dereference operator
                {
                    if (m_nodes.empty())
                        return NULL;
                    else
                        return m_nodes.back();
                }

                void operator++(int unused) // postfix operator
                {
                    (void)unused;

                    const BaseNode* node;
                    for(;;)
                    {
                        node = next();
                        // skip removed nodes, i.e. nodes with count==0
                        if (node == NULL || node->m_count != 0)
                            break;
                    }
                }

                // next for all nodes, including deleted ones with count==0
                const BaseNode* next()
                {
                    // preorder traversal with shallow stack
                    // nodes stack: path to node
                    // indexes stack: index of _next_ child
                    const BaseNode* node = m_nodes.back();
                    int index = m_indexes.back();

                    int level = get_level();
                    while (index >= m_root->get_num_children(node, level))
                    {
                        m_nodes.pop_back();
                        m_indexes.pop_back();
                        if (m_nodes.empty())
                            return NULL;

                        node = m_nodes.back();
                        index = ++m_indexes.back();
                        level = m_nodes.size()-1;
                        //printf ("back %d %d\n", node->word_id, index);
                    }
                    node = m_root->get_child_at(node, level, index);
                    m_nodes.push_back(node);
                    m_indexes.push_back(0);
                    //printf ("pushed %d %d %d\n", nodes.back()->word_id, index, indexes.back());
                    return node;
                }

                void get_ngram(std::vector<WordId>& ngram)
                {
                    ngram.resize(m_nodes.size()-1);
                    for(int i=1; i<(int)m_nodes.size(); i++)
                        ngram[i-1] = m_nodes[i]->m_word_id;
                }

                int get_level()
                {
                    return m_nodes.size()-1;
                }

                int at_root()
                {
                    return get_level() == 0;
                }

            private:
                const NGramTrie<TNODE, TBEFORELASTNODE, TLASTNODE>* m_root{};
                std::vector<const BaseNode*> m_nodes;   // path to node
                std::vector<int> m_indexes;       // index of _next_ child
        };

        NGramTrie::iterator begin() const
        {
            return NGramTrie::iterator(this);
        }


    public:
        NGramTrie(WordId wid = (WordId)-1)
        : TNODE(wid)
        {
            m_order = 0;
        }

        void set_order(int order)
        {
            this->m_order = order;
            clear();
        }

        void clear()
        {
            clear(this, 0);
            num_ngrams   = std::vector<int>(m_order, 0);
            total_ngrams = std::vector<int>(m_order, 0);
            TNODE::clear();
        }

        // Add increment to node->count
        int increment_node_count(BaseNode* node, const WordId* wids, int n,
                                 int increment)
        {
            total_ngrams[n-1] += increment;

            // Adding n-gram?
            if (node->m_count == 0 && increment > 0)
                num_ngrams[n-1]++;

            node->m_count += increment;

            // Removing n-gram?
            if (node->m_count <= 0 && increment < 0)
            {
                num_ngrams[n-1]--;

                // Control words must not be removed.
                if (n == 1 && wids[0] < NUM_CONTROL_WORDS)
                {
                    node->m_count = 1;
                }
            }
            return node->m_count;
        }

        BaseNode* add_node(const std::vector<WordId>& wids)
        {return add_node(&wids[0], wids.size());}
        BaseNode* add_node(const WordId* wids, int n);

        void get_probs_witten_bell_i(const std::vector<WordId>& history,
                                     const std::vector<WordId>& words,
                                     std::vector<double>& vp,
                                     int num_word_types);

        void get_probs_abs_disc_i(const std::vector<WordId>& history,
                                  const std::vector<WordId>& words,
                                  std::vector<double>& vp,
                                  int num_word_types,
                                  const std::vector<double>& Ds);

        // Get number of unique ngrams per level, excluding removed ones
        // with count==0.
        int get_num_ngrams(int level) { return num_ngrams[level]; }

        // Get total number of all ngram occurences per level.
        int get_total_ngrams(int level) { return total_ngrams[level]; }

        // Number of distinct words excluding removed ones with count=0.
        virtual int get_num_word_types() {return get_num_ngrams(0);}

        // Get number of occurences of a specific ngram.
        int get_ngram_count(const std::vector<WordId>& wids)
        {
            BaseNode* node = get_node(wids);
            if (node)
                return node->get_count();
            return 0;
        }

        BaseNode* get_node(const std::vector<WordId>& wids)
        {
            BaseNode* node = this;
            for (int i=0; i<(int)wids.size(); i++)
            {
                int index;
                node = get_child(node, i, wids[i], index);
                if (!node)
                    break;
            }
            return node;
        }

        int get_num_children(const BaseNode* node, int level) const
        {
            if (level == m_order)
                return 0;
            if (level == m_order - 1)
                return static_cast<const TBEFORELASTNODE*>(node)->m_children.size();
            return static_cast<const TNODE*>(node)->m_children.size();
        }

        int sum_child_counts(const BaseNode* node, int level) const
        {
            if (level == m_order)
                return -1;  // undefined for leaf nodes
            if (level == m_order - 1)
                return static_cast<const TBEFORELASTNODE*>(node)->sum_child_counts();
            return static_cast<const TNODE*>(node)->sum_child_counts();
        }

        BaseNode* get_child_at(BaseNode* parent, int level, int index)
        {
            if (level == m_order)
                return NULL;
            if (level == m_order - 1)
                return &static_cast<TBEFORELASTNODE*>(parent)->m_children[index];
            return static_cast<TNODE*>(parent)->m_children[index];
        }
        const BaseNode* get_child_at(const BaseNode* parent, int level, int index) const
        {
            if (level == m_order)
                return NULL;
            if (level == m_order - 1)
                return &static_cast<const TBEFORELASTNODE*>(parent)->m_children[index];
            return static_cast<const TNODE*>(parent)->m_children[index];
        }

        // Return the word ids of all direct child nodes,
        // excluding removed n-grams, i.g. count == 0.
        void get_child_wordids(const std::vector<WordId>& wids,
                          std::vector<WordId>& child_wids)
        {
            int level = wids.size();
            BaseNode* node = get_node(wids);
            if (node)
            {
                int num_children = get_num_children(node, level);
                for(int i=0; i<num_children; i++)
                {
                    BaseNode* child = get_child_at(node, level, i);
                    if (child->m_count)
                        child_wids.push_back(child->m_word_id);
                }
            }
        }

        int get_N1prx(const BaseNode* node, int level) const
        {
            if (level == m_order)
                return 0;
            if (level == m_order - 1)
                return static_cast<const TBEFORELASTNODE*>(node)->get_N1prx();
            return static_cast<const TNODE*>(node)->get_N1prx();
        }

        // -------------------------------------------------------------------
        // implementation specific
        // -------------------------------------------------------------------

        // reserve an exact number of items to avoid unessarily
        // overallocated memory when loading language models
        void reserve_unigrams(int count)
        {
            clear();
            TNODE::m_children.reserve(count);
        }


        // Estimate a lower bound for the memory usage of the whole trie.
        // This includes overallocations by std::vector, but excludes memory
        // used for heap management and possible heap fragmentation.
        uint64_t get_memory_size() const
        {
            NGramTrie::iterator it = begin();
            uint64_t sum = 0;
            for (; *it; it++)
                sum += get_node_memory_size(*it, it.get_level());
            return sum;
        }


    protected:
        void clear(BaseNode* node, int level)
        {
            if (level < m_order-1)
            {
                TNODE* tn = static_cast<TNODE*>(node);
                std::vector<BaseNode*>::iterator it;
                for (it=tn->m_children.begin(); it<tn->m_children.end(); it++)
                {
                    clear(*it, level+1);
                    if (level < m_order-2)
                        static_cast<TNODE*>(*it)->~TNODE();
                    else
                    if (level < m_order-1)
                        static_cast<TBEFORELASTNODE*>(*it)->~TBEFORELASTNODE();
                    MemFree(*it);

                }
                std::vector<BaseNode*>().swap(tn->m_children);  // really free the memory
            }
            TNODE::set_count(0);
        }


        BaseNode* get_child(BaseNode* parent, int level, int wid, int& index)
        {
            if (level == m_order)
                return NULL;
            if (level == m_order - 1)
                return static_cast<TBEFORELASTNODE*>(parent)->get_child(wid);
            return static_cast<TNODE*>(parent)->get_child(wid, index);
        }

        int get_node_memory_size(const BaseNode* node, int level) const
        {
            if (level == m_order)
                return sizeof(TLASTNODE);
            if (level == m_order - 1)
            {
                const TBEFORELASTNODE* nd = static_cast<const TBEFORELASTNODE*>(node);
                return sizeof(TBEFORELASTNODE) +
                       sizeof(TLASTNODE) *
                       (nd->m_children.capacity() - nd->m_children.size());
            }

            const TNODE* nd = static_cast<const TNODE*>(node);
            return sizeof(TNODE) +
                   sizeof(TNODE*) * nd->m_children.capacity();
        }


    public:
        int m_order;

        // Keep track of these counts to avoid
        // traversing the tree for these numbers.
        //
        // Number of unique ngrams with count > 0, per level.
        std::vector<int> num_ngrams;

        // Number of total occurences of all n-grams, per level.
        std::vector<int> total_ngrams;
};

#pragma pack()


//------------------------------------------------------------------------
// DynamicModelBase - non-template abstract base class of all DynamicModels
//------------------------------------------------------------------------
class DynamicModelBase : public NGramModel
{
    public:
        // iterator for template-free, polymorphy based ngram traversel
        class ngrams_iter
        {
            public:
                virtual ~ngrams_iter() {}
                virtual const BaseNode* operator*() const = 0;
                virtual void operator++(int unused) = 0;
                virtual void get_ngram(std::vector<WordId>& ngram) = 0;
                virtual int get_level() = 0;
                virtual bool at_root() = 0;
        };

    public:
        using Super = NGramModel;
        DynamicModelBase(OrderType order=0) :
            Super(order)
        {
        }

        virtual std::unique_ptr<DynamicModelBase::ngrams_iter> ngrams_begin() const = 0;

        virtual void clear()
        {
            LanguageModel::clear();
            assure_valid_control_words();
        }

        virtual std::unique_ptr<DynamicModelBase> clone_empty() = 0;

        // Make sure control words exist as unigrams.
        // They must have a count of at least 1. 0 means removed and
        // it also throws off the normalization of witten-bell smoothing.
        virtual void assure_valid_control_words();

        // Copy contents of model to self. The order of self
        // stays unchanged. Moved here from lm_wrapper.py.
        virtual void copy_contents(const DynamicModelBase* model);

        virtual int  get_ngram_count(const char* const* ngram, int n) = 0;
        virtual int  get_ngram_count(const wchar_t* const* ngram, int n) = 0;
        virtual void get_node_values(const BaseNode* node, int level,
                                     std::vector<int>& values) const = 0;

        BaseNode* count_ngram(const std::vector<std::wstring>& ngram,
                              int increment=1, bool allow_new_words=true)
        {
            const size_t n = ngram.size();
            std::vector<const wchar_t*> a(n);
            for (size_t i=0; i<n; i++)
                a[i] = ngram[i].data();
            return count_ngram(a, increment, allow_new_words);
        }
        BaseNode* count_ngram(const std::vector<std::wstring_view>& ngram,
                              int increment=1, bool allow_new_words=true)
        {
            const size_t n = ngram.size();
            std::vector<const wchar_t*> a(n);
            for (size_t i=0; i<n; i++)
                a[i] = ngram[i].data();
            return count_ngram(a, increment, allow_new_words);
        }

        virtual BaseNode* count_ngram(const std::vector<std::string>& ngram,
                                      int increment, bool allow_new_words=true) = 0;
        virtual BaseNode* count_ngram(const std::vector<const char*>& ngram,
                                      int increment, bool allow_new_words=true) = 0;
        virtual BaseNode* count_ngram(const char* const* ngram, size_t n,
                                      int increment, bool allow_new_words=true) = 0;
        virtual BaseNode* count_ngram(const std::vector<const wchar_t*> ngram,
                                int increment=1, bool allow_new_words=true) = 0;
        virtual BaseNode* count_ngram(const WordId* wids,
                                      int n, int increment) = 0;

        virtual Smoothing get_smoothing() = 0;
        virtual void set_smoothing(Smoothing s) = 0;

        // throw exceptions, record errors
        void load(const std::string& filename) {load(filename.c_str());}
        void save(const std::string& filename) {save(filename.c_str());}
        virtual void load(const char* filename) override;
        virtual void save(const char* filename) override;

        // don't throw exceptions, low level
        virtual LMError do_load(const char* filename) override
        {return load_arpac(filename);}
        virtual LMError do_save(const char* filename) override
        {return save_arpac(filename);}

        virtual LMError get_load_error() override
        {return m_load_error;}

        virtual std::string get_load_error_msg() override
        {return m_load_error_msg;}
        virtual void set_load_error_msg(const std::string& msg) override
        {m_load_error_msg = msg;}

        virtual bool is_modified() override
        {return m_modified;}
        virtual void set_modified(bool modified) override
        {m_modified = modified;}

        // Debug output, dump all n-grams.
        virtual void dump();


        // former functions of lm_wrapper.py

        template<class F>
        void for_each_ngram(const F& func)
        {
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

                words.clear();
                m_dictionary.ids_to_words(words, wids);

                values.clear();
                get_node_values(node, level, values);

                func(words, values);
            }
        }

        // Return number of n-gram types and total occurances
        // for each n-gram level.
        std::tuple<std::vector<size_t>, std::vector<size_t>> get_counts()
        {
            size_t order_ = static_cast<size_t>(get_order());
            std::vector<size_t> counts(order_, 0);
            std::vector<size_t> totals(order_, 0);
            for_each_ngram([&](const std::vector<const char*>& ngram,
                               const std::vector<int>& values)
            {
                counts.at(ngram.size()-1)++;
                totals.at(ngram.size()-1) += static_cast<size_t>(values.at(0));
            });
            return {counts, totals};
        }

        // Return the contents of the model as one (potentially huge) vector.
        // Only used in unit tests. Elswhere use for_each_ngram instead.
        NGramContents get_contents()
        {
            NGramContents contents;

            for_each_ngram([&](const std::vector<const char*>& ngram,
                               const std::vector<int>& values)
            {
                contents.emplace_back();
                to_string(std::get<0>(contents.back()), ngram);
                std::get<1>(contents.back()) = values;
            });
            return contents;
        }
        // Return a copy of self with all ngrams removed whose
        // count is less or equal to <prune_count>.
        // prune_count==-1  // prune all frequencies
        // prune_count=0    // prune nothing
        // prune_count>0    // prune frequencies below or equal prune_count
        std::unique_ptr<DynamicModelBase> prune(const std::vector<int>& prune_counts);

        // Convenience function to convert to wstrings. Data structures in lm are
        // based on wchar_t strings, so for now this has to be done anyway.
        virtual void learn_tokens(const std::vector<UString>& utokens, bool allow_new_words=true);

        // Extract n-grams from tokens and count them.
        virtual void learn_tokens(const std::vector<std::string>& tokens, bool allow_new_words=true)
        {
            std::vector<const char*> ngram;
            for_each_ngram_in(tokens, get_order(), ngram, [&]
            {
                count_ngram(ngram, 1, allow_new_words);
            });

            this->m_modified = true;
        }

        // Remove word context[-1] where it appears after history context[:-1]
        // from the model. If the history is empty all n-grams containing word
        // will be removed.
        void remove_context(std::map<std::vector<std::string>, int>& changes,
                            const std::vector<UString>& context)
        {
            std::vector<std::string> scontext;
            to_string(scontext, context);
            remove_context(changes, scontext);
        }
        void remove_context(const std::vector<std::string>& context)
        {
            std::map<std::vector<std::string>, int> changes;
            remove_context(changes, context);
        }
        void remove_context(std::map<std::vector<std::string>, int>& changes,
                            const std::vector<std::string>& context)
        {
            get_remove_context_changes(changes, context);
            if (!changes.empty())
            {
                for (auto it : changes)
                    count_ngram(it.first, it.second);

                m_modified = true;
            }
        }

        // Simulate removal of context.
        // Returns a dict of affected n-grams and their count changes (negative).
        void get_remove_context_changes(std::map<std::vector<std::string>, int>& changes,
                                        const std::vector<std::string>& context)
        {
            for_each_ngram([&](const std::vector<const char*>& ngram,
                               const std::vector<int>& values)
            {
                int count = values.at(0);

                // find intersection with context
                for (size_t i=0; i<ngram.size(); i++)
                {
                    size_t k = std::min(context.size(), i+1);
                    size_t j;
                    for (j=0; j<k; j++)
                        if (ngram.at(i-j) != context.at(context.size()-j-1))
                            break;

                    if (j >= k)  // no break hit?
                    {
                        // -1 removed: python for does not increase index past range!
                        if (j == context.size())
                        {
                            std::vector<std::string> sngram;
                            to_string(sngram, ngram);
                            changes[sngram] = -count;
                            break;
                        }
                    }
                }
            });
        }

    protected:
        // temporary unigram, only used during loading
        typedef struct
        {
            std::wstring word;
            uint32_t count;
            uint32_t time;
        } Unigramw;
        typedef struct
        {
            std::string word;
            uint32_t count;
            uint32_t time;
        } Unigram;
        virtual LMError set_unigrams(const std::vector<Unigram>& unigrams);

        virtual LMError write_arpa_ngram(FILE* f,
                                       const BaseNode* node,
                                       const std::vector<WordId>& wids)
        {
            fwprintf(f, L"%d", node->get_count());

            std::vector<WordId>::const_iterator it;
            for(it = wids.begin(); it != wids.end(); it++)
                fwprintf(f, L" %ls", id_to_word(*it));

            fwprintf(f, L"\n");

            return ERR_NONE;
        }
        virtual LMError write_arpa_ngrams(FILE* f);

        virtual LMError load_arpac(const char* filename);
        virtual LMError save_arpac(const char* filename);

        virtual void set_node_time(BaseNode* node, uint32_t time)
        {
            (void) node;
            (void) time;
        }
        virtual int get_num_ngrams(int level) = 0;
        virtual void reserve_unigrams(int count) = 0;

        // Number of distinct words excluding removed ones with count=0.
        virtual int get_num_word_types() {return get_num_ngrams(0);}

    public:
        int m_modified{false};
        LMError m_load_error{LMError::ERR_NONE};
        std::string m_load_error_msg;
};


//------------------------------------------------------------------------
// DynamicModel - dynamically updatable language model
//------------------------------------------------------------------------
template <class TNGRAMS>
class _DynamicModel : public DynamicModelBase
{
    public:
        static const Smoothing DEFAULT_SMOOTHING = ABS_DISC_I;

        class ngrams_iter : public DynamicModelBase::ngrams_iter
        {
            public:
                ngrams_iter(const _DynamicModel<TNGRAMS>* lm)
                : it(&lm->ngrams)
                {}

                virtual const BaseNode* operator*() const // dereference operator
                { return *it; }

                virtual void operator++(int unused) // postfix operator
                { (void) unused; it++; }

                virtual void get_ngram(std::vector<WordId>& ngram)
                { it.get_ngram(ngram); }

                virtual int get_level()
                { return it.get_level(); }

                virtual bool at_root()
                { return it.at_root(); }

            public:
                typename TNGRAMS::iterator it;
        };
        virtual std::unique_ptr<DynamicModelBase::ngrams_iter> ngrams_begin() const override
        {return std::make_unique<ngrams_iter>(this);}

    public:
        _DynamicModel(OrderType order=3)
        {
            m_smoothing = DEFAULT_SMOOTHING;
            set_order(order);
        }

        virtual ~_DynamicModel()
        {
            #ifdef LMDEBUG
            uint64_t v = dictionary.get_memory_size();
            uint64_t n = ngrams.get_memory_size();
            printf("memory: dictionary=%ld, ngrams=%ld, total=%ld\n", v, n, v+n);
            #endif

            clear();
        }

        virtual std::unique_ptr<DynamicModelBase> clone_empty() override
        {
            std::unique_ptr<_DynamicModel> model = std::make_unique<_DynamicModel>();
            model->set_order(this->m_order);
            model->m_smoothing = this->m_smoothing;
            return model;
        }

        virtual void clear();
        virtual void set_order(int n);
        virtual Smoothing get_smoothing() override {return m_smoothing;}
        virtual void set_smoothing(Smoothing s) override {m_smoothing = s;}

        virtual std::vector<Smoothing> get_smoothings()
        {
            std::vector<Smoothing> smoothings;
            smoothings.push_back(WITTEN_BELL_I);
            smoothings.push_back(ABS_DISC_I);
            return smoothings;
        }

        virtual void filter_candidates(const std::vector<WordId>& in,
                                             std::vector<WordId>& out)
        {
            // filter out removed unigrams
            int num_candidates = in.size();
            out.reserve(num_candidates);
            for (int i=0; i<num_candidates; i++)
            {
                WordId wid = in[i];
                // can crash if is_model_valid() == false
                BaseNode* node = ngrams.get_child_at(&ngrams, 0, wid);
                if (node->get_count())
                    out.push_back(wid);
            }
        }

        // Plausibilty check befor predict() calls can be performed.
        //
        // When count_ngram() is called manually, it isn't guaranteed
        // that all nodes are created that are required for a valid model.
        // Unigrams in particular may be missing. This can lead to crashes
        // in filter_candidates or probability calculations.
        //
        // For filling models it's best to avoid count_ngram() and
        // use the safer learn_tokens() instead.
        virtual bool is_model_valid()
        {
            // including removed unigrams with count==0
            int num_unigrams = ngrams.get_num_children(&ngrams, 0);
            return num_unigrams == m_dictionary.get_num_word_types();
        }

        virtual BaseNode* count_ngram(const std::vector<std::string>& ngram,
                                      int increment, bool allow_new_words=true) override;
        virtual BaseNode* count_ngram(const std::vector<const char*>& ngram,
                                      int increment, bool allow_new_words=true) override;
        virtual BaseNode* count_ngram(const char* const* ngram, size_t n,
                                      int increment, bool allow_new_words=true) override;
        virtual BaseNode* count_ngram(const std::vector<const wchar_t*> ngram,
                                int increment=1, bool allow_new_words=true) override;

        virtual BaseNode* count_ngram(const WordId* wids, int n, int increment)  override;
        virtual int get_ngram_count(const char* const* ngram, int n);
        virtual int get_ngram_count(const wchar_t* const* ngram, int n);

        virtual void get_node_values(const BaseNode* node, int level,
                                     std::vector<int>& values) const override
        {
            values.push_back(node->m_count);
            values.push_back(ngrams.get_N1prx(node, level));
        }
        virtual void get_memory_sizes(std::vector<long>& values)
        {
            values.push_back(m_dictionary.get_memory_size());
            values.push_back(ngrams.get_memory_size());
        }

    protected:
        virtual LMError write_arpa_ngrams(FILE* f);

        virtual void get_words_with_predictions(
                                       const std::vector<WordId>& history,
                                       std::vector<WordId>& wids)
        {
            std::vector<WordId> h(history.end()-1, history.end()); // bigram history
            ngrams.get_child_wordids(h, wids);
        }

        virtual void get_probs(const std::vector<WordId>& history,
                               const std::vector<WordId>& words,
                               std::vector<double>& probabilities);

        virtual int increment_node_count(BaseNode* node, const WordId* wids,
                                         int n, int increment)
        {
            return ngrams.increment_node_count(node, wids, n, increment);
        }

        // Number of n-grams per level, excluding removed ones with count==0.
        virtual int get_num_ngrams(int level)
        {
            return ngrams.get_num_ngrams(level);
        }

        virtual void reserve_unigrams(int count)
        {
            ngrams.reserve_unigrams(count);
        }

   private:
        BaseNode* get_ngram_node(const char* const* ngram, size_t n)
        {
            std::vector<WordId> wids(n);
            for (size_t i=0; i<n; i++)
                wids[i] = m_dictionary.word_to_id(ngram[i]);
            return ngrams.get_node(wids);
        }
        BaseNode* get_ngram_node(const wchar_t* const* ngram, size_t n)
        {
            std::vector<WordId> wids(n);
            for (size_t i=0; i<n; i++)
                wids[i] = m_dictionary.word_to_id(ngram[i]);
            return ngrams.get_node(wids);
        }

    protected:
        // n-gram trie
        TNGRAMS ngrams;

        // smoothing
        Smoothing m_smoothing;

        // total number of n-grams with exactly one count, per level
        std::vector<int> m_n1s;

        // total number of n-grams with exactly two counts, per level
        std::vector<int> m_n2s;

        // discounting parameters for abs. discounting, kneser-ney, per level
        std::vector<double> m_Ds;
};

typedef _DynamicModel<NGramTrie<TrieNode<BaseNode>,
                      BeforeLastNode<BaseNode, LastNode<BaseNode> >,
                      LastNode<BaseNode> > > DynamicModel;

} // namespace


#include "lm_dynamic_impl.h"


#endif

