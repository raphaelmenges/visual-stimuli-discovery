//! Feature vector computes multiple features.
/*!
Is threadsafe, can be executed multiple times in parallel.
*/

#pragma once

#include <opencv2/core/types.hpp>
#include <map>
#include <memory>

namespace feature
{
	class FeatureVector
	{
	public:

		// Constructor (expects matrices with BGRA and 8bit per channel)
		FeatureVector(
			std::shared_ptr<const cv::Mat> a,
			std::shared_ptr<const cv::Mat> b);

		// Get features
		std::map<std::string, double> get() const
		{
			return _features;
		}

		// Get names of features
		std::vector<std::string> get_names() const
		{
			std::vector<std::string> names;
			for (const auto& r_entry : _features)
			{
				names.push_back(r_entry.first);
			}
			return names;
		}

		// Get times
		std::map<std::string, int> get_times() const
		{
			return _times;
		}

	private:

		// Map with key as feature string and value as feature value
		std::map<std::string, double> _features;

		// Map with key as feature or descriptor string and value as milliseconds required for computation
		std::map<std::string, int> _times;
	};
}
