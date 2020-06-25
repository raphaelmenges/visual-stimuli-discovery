//! Sift match feature.
/*!
Calculation of sift match-based differences.
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// Sift match feature
	class SiftMatch : public Interface<const cv::Mat>
	{
	public:

		// Constructor. Expects both matrices to have same size etc.
		SiftMatch(
			std::shared_ptr<const cv::Mat> a,
			std::shared_ptr<const cv::Mat> b);
	};
}
