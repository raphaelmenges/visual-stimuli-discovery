#pragma once

#include <Data/LogDatum.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>
#include <libsimplewebm.hpp>
#include <memory>
#include <string>
#include <vector>

// do not serialize, too much data if stored as raw image
// holds viewport pixels from screencast according to information from the datacast (e.g., maybe crop viewport from screenshot)
// holds 8bit BGRA pixel data

namespace data
{
	class LogImage
	{
	public:

		// Constructor. Copies pixel data of screenshot into member structure
		LogImage(const simplewebm::Image& r_screenshot, std::shared_ptr<const LogDatum> sp_log_datum);

		// Get pixels of viewport
		const cv::Mat get_viewport_pixels() const
		{
			return _viewport_pixels;
		}
		
		// Get pixels of viewport in gray
		const cv::Mat get_viewport_pixels_gray() const
		{
			return _viewport_pixels_gray;
		}

		// Get pixels of a layer as 4 channel 8 bit depth image. Size of matrix is viewport size, alpha value of non-layer pixels is zero.
		// Frame of layer must match with frame of log image
		const cv::Mat get_layer_pixels(const cv::Mat layer_view_mask) const;

	private:

		cv::Mat _viewport_pixels; // pixels of the viewport
		cv::Mat _viewport_pixels_gray; // pixels of the viewport in gray
	};
}
