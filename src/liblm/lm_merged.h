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

#ifndef LM_MERGED_H
#define LM_MERGED_H

#include <vector>
#include "lm.h"

namespace lm {

//------------------------------------------------------------------------
// MergedModel - abstract container for one or more component language models
//------------------------------------------------------------------------

struct map_wstr_cmp
{
  bool operator() (const std::wstring& lhs, const std::wstring& rhs) const
  { return lhs < rhs; }
};
typedef std::map<std::wstring, double, map_wstr_cmp> ResultsMap;
//#include <unordered_map>
//typedef std::unordered_map<const wchar_t*, double> ResultsMap;

class MergedModel : public LanguageModel
{
    public:
        using Super = LanguageModel;

        // language model overloads
        virtual bool is_model_valid() override
        {
            for (unsigned i=0; i<components.size(); i++)
                if (!components[i]->is_model_valid())
                    return false;
            return true;
        }

        virtual void predict(std::vector<UPredictResult>& uresults,
                             const std::vector<UString>& ucontext,
                             std::optional<size_t> limit={},
                             PredictOptions options = DEFAULT_OPTIONS) override
        {
            Super::predict(uresults, ucontext, limit, options);
        }
        virtual void predict(std::vector<PredictResult>& results,
                             const std::vector<const wchar_t*>& context,
                             int limit=-1,
                             PredictOptions options = DEFAULT_OPTIONS) override;

        virtual LMError get_load_error() override
        {
            return {};
        }

        virtual void load(const char* filename) override
        {
            (void)filename;
        }
        virtual void save(const char* filename) override
        {
            (void)filename;
        }

        virtual LMError do_load(const char* filename) override
        {
            (void)filename;
            return ERR_NOT_IMPL;
        }
        virtual LMError do_save(const char* filename) override
        {
            (void)filename;
            return ERR_NOT_IMPL;
        }

        virtual std::string get_load_error_msg() override
        {
            return {};
        }
        virtual void set_load_error_msg(const std::string& msg) override
        {
            (void) msg;
        }

        virtual bool is_modified() override
        {
            return {};
        }
        virtual void set_modified(bool modified) override
        {
            (void) modified;
        }

        // merged model interface
        virtual void set_models(const std::vector<LanguageModel*>& models)
        { components = models;}

    protected:
        // merged model interface
        virtual void init_merge() {}
        virtual bool can_limit_components() {return false;}
        virtual void merge(ResultsMap& dst, const std::vector<PredictResult>& values,
                                      int model_index) = 0;
        virtual bool needs_normalization() {return false;}

    private:
        void normalize(std::vector<PredictResult>& results, int result_size);

    protected:
        std::vector<LanguageModel*> components;
};

//------------------------------------------------------------------------
// OverlayModel - merge by overlaying language models
//------------------------------------------------------------------------

class OverlayModel : public MergedModel
{
    protected:
        virtual void merge(ResultsMap& dst, const std::vector<PredictResult>& values,
                                      int model_index);

        // overlay can safely use a limit on prediction results
        // for component models
        virtual bool can_limit_components() {return true;}

        virtual bool needs_normalization() {return true;}
};

//------------------------------------------------------------------------
// LinintModel - linearly interpolate language models
//------------------------------------------------------------------------

class LinintModel : public MergedModel
{
    public:
        void set_weights(const std::vector<double>& weights)
        { m_weights = weights; }

        virtual void init_merge();
        virtual void merge(ResultsMap& dst, const std::vector<PredictResult>& values,
                                      int model_index);
        virtual double get_probability(const wchar_t* const* ngram, int n);

    protected:
        std::vector<double> m_weights;
        double m_weight_sum;
};


//------------------------------------------------------------------------
// LoglinintModel - log-linear interpolation of language models
//------------------------------------------------------------------------

class LoglinintModel : public MergedModel
{
    public:
        void set_weights(const std::vector<double>& weights)
        { this->m_weights = weights; }

        virtual void init_merge();
        virtual void merge(ResultsMap& dst, const std::vector<PredictResult>& values,
                                      int model_index);

        // there appears to be no simply way to for direct normalized results
        // -> run normalization explicitly
        virtual bool needs_normalization() {return true;}
    protected:
        std::vector<double> m_weights;
};

}  // namespace

#endif

