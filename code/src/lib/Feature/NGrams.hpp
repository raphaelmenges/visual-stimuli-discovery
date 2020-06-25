//! N-grams feature.
/*!
Calculation of similarity of character-wise n-grams
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// N-grams feature
	class NGrams : public Interface<std::vector<std::string> >
	{
	public:

		// Constructor
		NGrams(
			std::shared_ptr<const std::vector<std::string> > a,
			std::shared_ptr<const std::vector<std::string> > b);
	};
}
