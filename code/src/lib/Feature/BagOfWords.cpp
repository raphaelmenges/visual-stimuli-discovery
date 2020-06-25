#include "BagOfWords.hpp"

const int MIN_WORD_LENGTH = core::mt::get_config_value(3, { "feature", "bag_of_words", "min_word_length" });

namespace feature
{
	BagOfWords::BagOfWords(
		std::shared_ptr<const std::vector<std::string> > a,
		std::shared_ptr<const std::vector<std::string> > b)
		:
		Interface(a, b)
	{
		// Filter for ASCII and words longer than a preset, only (otherwise, many strange words are considered)
		auto ascii_a = std::make_shared<std::vector<std::string> >();
		auto ascii_b = std::make_shared<std::vector<std::string> >();
		for (const auto& r_word : *a)
		{
			if (core::misc::is_ascii(r_word) && ((int)r_word.length() >= MIN_WORD_LENGTH)) // this length check only works for ASCII characters
			{
				ascii_a->push_back(r_word);
			}
		}
		for (const auto& r_word : *b)
		{
			if (core::misc::is_ascii(r_word) && ((int)r_word.length() >= MIN_WORD_LENGTH)) // this length check only works for ASCII characters
			{
				ascii_b->push_back(r_word);
			}
		}

		// Filter for unique words
		auto sp_unique_a = core::misc::get_unique_strings(ascii_a);
		auto sp_unique_b = core::misc::get_unique_strings(ascii_b);

		// Combine both vectors to the vocabulary
		auto sp_vocabulary = std::make_shared<std::vector<std::string> >(*sp_unique_a);
		sp_vocabulary->insert(sp_vocabulary->end(), sp_unique_b->begin(), sp_unique_b->end());
		sp_vocabulary = core::misc::get_unique_strings(sp_vocabulary);

		// How many terms are different in both?
		int unique_diff_a = (int)sp_vocabulary->size() - (int)sp_unique_a->size();
		int unique_diff_b = (int)sp_vocabulary->size() - (int)sp_unique_b->size();
		int unique_diff = unique_diff_a + unique_diff_b;
		_features["bag_of_words_unique_terms_count"] = (double)unique_diff; // how many unique terms are there

		// Go over each word and erase pairs
		std::vector<bool> lonely_a(ascii_a->size(), true);
		std::vector<bool> lonely_b(ascii_b->size(), true);
		for (int i = 0; i < (int)ascii_a->size(); ++i)
		{
			for (int j = 0; j < (int)ascii_b->size(); ++j)
			{
				if (lonely_a.at(i) && lonely_b.at(j)) // be sure that one partner has not already been with another word
				{
					if (ascii_a->at(i) == ascii_b->at(j)) // compare words
					{
						lonely_a[i] = false;
						lonely_b[j] = false;
					}
				}
			}
		}

		// Count lonely words
		core::long64 count = 0;
		for (const auto& r_lonely : lonely_a)
		{
			if (r_lonely) { ++count; }
		}
		for (const auto& r_lonely : lonely_b)
		{
			if (r_lonely) { ++count; }
		}
		_features["bag_of_words_diff"] = (double)count; // how many words are different in both images

		// Further features that might be considered
		_features["bag_of_words_vocabulary_size"] = (double)sp_vocabulary->size();

		/*
		// Manual debugging
		core::mt::log_info("Bag of words feature");
		for (int i = 0; i < (int)ascii_a->size(); ++i)
		{
			core::mt::log_info(ascii_a->at(i));
		}
		core::mt::log_info("+++");
		for (int i = 0; i < (int)ascii_b->size(); ++i)
		{
			core::mt::log_info(ascii_b->at(i));
		}
		core::mt::log_info("---");
		*/
	}
}
