//! Histogram descriptor.
/*!
Just stores histogram observations of an inputted image.
*/

#pragma once

#include <opencv2/core/types.hpp>
#include <memory>

namespace descriptor
{
	// Histogram descriptor
	class Histogram
	{
	public:

		// Constructor
		Histogram(std::shared_ptr<const cv::Mat> sp_image);

		// Getter
		std::shared_ptr<const cv::Mat> get_blue_hist()			const { return _sp_blue_hist; }
		std::shared_ptr<const cv::Mat> get_green_hist()			const { return _sp_green_hist; }
		std::shared_ptr<const cv::Mat> get_red_hist()			const { return _sp_red_hist; }
		std::shared_ptr<const cv::Mat> get_hue_hist()			const { return _sp_hue_hist; }
		std::shared_ptr<const cv::Mat> get_saturation_hist()	const { return _sp_saturation_hist; }
		std::shared_ptr<const cv::Mat> get_lightness_hist()		const { return _sp_lightness_hist; }
		std::shared_ptr<const cv::Mat> get_gray_hist()			const { return _sp_gray_hist; }

	private:

		// Members
		std::shared_ptr<cv::Mat> _sp_blue_hist = std::make_shared<cv::Mat>();
		std::shared_ptr<cv::Mat> _sp_green_hist = std::make_shared<cv::Mat>();
		std::shared_ptr<cv::Mat> _sp_red_hist = std::make_shared<cv::Mat>();
		std::shared_ptr<cv::Mat> _sp_hue_hist = std::make_shared<cv::Mat>();
		std::shared_ptr<cv::Mat> _sp_saturation_hist = std::make_shared<cv::Mat>();
		std::shared_ptr<cv::Mat> _sp_lightness_hist = std::make_shared<cv::Mat>();
		std::shared_ptr<cv::Mat> _sp_gray_hist = std::make_shared<cv::Mat>();
	};
}
