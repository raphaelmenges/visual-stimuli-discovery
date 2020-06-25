//! Pixel-diff feature.
/*!
Calculation of pixel-based differences.
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// Pixel difference feature
	class PixelDiff : public Interface<const cv::Mat>
	{
	public:

		// Constructor. Expects both matrices to have same size etc.
		PixelDiff(
			std::shared_ptr<const cv::Mat> a,
			std::shared_ptr<const cv::Mat> b);
	};
}
