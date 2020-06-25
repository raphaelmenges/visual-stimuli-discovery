//! MSSIM feature.
/*!
Calculation of mean structural similarity.
*/

#pragma once

#include <Feature/Feature.hpp>

namespace feature
{
	// MSSIM feature
	class MSSIM : public Interface<const cv::Mat>
	{
	public:

		// Constructor. Expects both matrices to have same size etc.
		MSSIM(
			std::shared_ptr<const cv::Mat> a,
			std::shared_ptr<const cv::Mat> b);
	};
}
