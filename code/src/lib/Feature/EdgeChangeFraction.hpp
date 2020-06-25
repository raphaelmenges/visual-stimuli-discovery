//! Edge change fraction feature.
/*!
Calculation of edge change fraction-based differences.
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// Edge change fraction feature
	class EdgeChangeFraction : public Interface<const cv::Mat>
	{
	public:

		// Constructor. Expects both matrices to have same size etc.
		EdgeChangeFraction(
			std::shared_ptr<const cv::Mat> a,
			std::shared_ptr<const cv::Mat> b);
	};
}
