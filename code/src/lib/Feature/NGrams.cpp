#include "NGrams.hpp"

bool invalid_char(char c) 
{
	return !(c >= 0 && c < 128);
}

void strip_unicode(std::string & str) 
{
	str.erase(std::remove_if(str.begin(),str.end(), invalid_char), str.end());  
}

std::shared_ptr<std::vector<std::string> > generate_ngrams(std::shared_ptr<const std::vector<std::string> > sp_words, int n)
{
	auto sp_ngrams = std::make_shared<std::vector<std::string> >();
	for(const auto& r_word : *sp_words)
	{
		std::string w = r_word;
	
		// Filter for ASCII letters
		strip_unicode(w);
		
		// Filter for strings at least as long as n
		if((int)w.length() < n) { continue; } // this length check only works for ASCII characters
		
		// Create n-grams
		for (int i = 0; i <= (int)w.length() - n; ++i)
		{
			sp_ngrams->push_back(w.substr(i, n));
		}
	}
	return sp_ngrams;
}

namespace feature
{
	NGrams::NGrams(
		std::shared_ptr<const std::vector<std::string> > a,
		std::shared_ptr<const std::vector<std::string> > b)
		:
		Interface(a, b)
	{
		// N of n-gram
		const int n = 3;
		
		// Generate ASCII based n-grams
		auto sp_ngrams_a = generate_ngrams(a, n);
		auto sp_ngrams_b = generate_ngrams(b, n);
		
		// Make n-grams unique
		auto sp_unique_a = core::misc::get_unique_strings(sp_ngrams_a);
		auto sp_unique_b = core::misc::get_unique_strings(sp_ngrams_b);
		
		// Combine both vectors to the vocabulary
		auto sp_vocabulary = std::make_shared<std::vector<std::string> >(*sp_unique_a);
		sp_vocabulary->insert(sp_vocabulary->end(), sp_unique_b->begin(), sp_unique_b->end());
		sp_vocabulary = core::misc::get_unique_strings(sp_vocabulary);
		_features["n_grams_vocabulary_size"] = (int)sp_vocabulary->size();
		
		// Go over the n-grams and find matches (all grams are unique within their containing vector)
		int match_count = 0;
		for(const auto& g_a : *sp_unique_a)
		{
			for(const auto& g_b : *sp_unique_b)
			{
				if(g_a == g_b)
				{
					++match_count;
				}
			}
		}
		_features["n_grams_match_count"] = match_count;
		
		// How many n-grams are there minimum and maximum?
		_features["n_grams_min_count"] = std::min((int)sp_unique_a->size(), (int)sp_unique_b->size());
		_features["n_grams_max_count"] = std::max((int)sp_unique_a->size(), (int)sp_unique_b->size());
		
		// Ratio of matches
		if(_features["n_grams_min_count"] > 0)
		{
			_features["n_grams_match_ratio"] = _features["n_grams_match_count"] / _features["n_grams_min_count"]; // match count can be at highest as many as min count
		}
		else
		{
			_features["n_grams_match_ratio"] = 0;
		}
		
		// Jaccard Similarity
		double denominator = ((double)sp_unique_a->size() + (double)sp_unique_b->size() - (double)match_count);
		if (denominator > 0.0)
		{
			_features["n_grams_jaccard"] = (double)match_count / denominator;
		}
		else
		{
			_features["n_grams_jaccard"] = 0;
		}

		// TODO: Cosinus similarity on the n-grams?
	}
}
