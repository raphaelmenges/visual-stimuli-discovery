//! Histogram feature.
/*!
Calculation of difference between histograms.
*/

#pragma once

#include <Feature/Feature.hpp>
#include <Descriptor/Histogram.hpp>

namespace feature
{
	// Histogram feature
	class Histogram: public Interface<const descriptor::Histogram>
	{
	public:

		// Constructor
		Histogram(
			std::shared_ptr<const descriptor::Histogram> a,
			std::shared_ptr<const descriptor::Histogram> b);
	};
}
