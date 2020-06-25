//! Bag-of-words feature.
/*!
Calculation of difference between bag-of-words.
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// Bag-of-words feature
	class BagOfWords : public Interface<std::vector<std::string> >
	{
	public:

		// Constructor
		BagOfWords(
			std::shared_ptr<const std::vector<std::string> > a,
			std::shared_ptr<const std::vector<std::string> > b);
	};
}
