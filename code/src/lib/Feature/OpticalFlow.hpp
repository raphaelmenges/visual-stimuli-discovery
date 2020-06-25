//! Optical flow feature.
/*!
Calculation of optical flow-based differences.
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// Optical flow feature
	class OpticalFlow : public Interface<const cv::Mat>
	{
	public:

		// Constructor. Expects both matrices to have same size etc.
		OpticalFlow(
			std::shared_ptr<const cv::Mat> a,
			std::shared_ptr<const cv::Mat> b);
	};
}
