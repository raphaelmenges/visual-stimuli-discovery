//! PSNR feature.
/*!
Calculation of peak signal-to-noise ratio.
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// PSNR feature
	class PSNR : public Interface<const cv::Mat>
	{
	public:

		// Constructor. Expects both matrices to have same size etc.
		PSNR(
			std::shared_ptr<const cv::Mat> a,
			std::shared_ptr<const cv::Mat> b);
	};
}
