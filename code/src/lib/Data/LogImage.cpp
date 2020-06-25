#include "LogImage.hpp"
#include <Core/Core.hpp>

namespace data
{
	LogImage::LogImage(const simplewebm::Image& r_screenshot, std::shared_ptr<const LogDatum> sp_log_datum)
	{
		// Prepare some variables
		cv::Rect viewport_rect( // raw viewport coordinates (in screen coordinates)
			sp_log_datum->get_viewport_pos().x,
			sp_log_datum->get_viewport_pos().y,
			sp_log_datum->get_viewport_width(),
			sp_log_datum->get_viewport_height());
		cv::Rect viewport_in_screen_rect; // rect of viewport in screenshot (in screen coordinates)

		// Estimate upper left position of viewport
		int x = viewport_rect.x;
		int y = viewport_rect.y;
		x = core::math::clamp(x, 0, r_screenshot.width - 1);
		y = core::math::clamp(y, 0, r_screenshot.height - 1);

		// Estimate visible width of viewport
		int far_x = viewport_rect.x + viewport_rect.width - 1;
		far_x = core::math::clamp(far_x, 0, r_screenshot.width - 1);
		int width = far_x - x + 1;

		// Estimate visible height of viewport
		int far_y = viewport_rect.y + viewport_rect.height - 1;
		far_y = core::math::clamp(far_y, 0, r_screenshot.height - 1);
		int height = far_y - y + 1;

		// Create rect covering the viewport
		viewport_in_screen_rect = cv::Rect(x, y, width, height); // geometrical coordinate system, not mathematical

		// Convert image data to matrix data containing screen pixels
		cv::Mat screen_pixels = cv::Mat(
			r_screenshot.height,
			r_screenshot.width,
			CV_8UC3,
			const_cast<char *>(r_screenshot.data.data()));

		// Crop and convert (and copy) pixels of viewport. Turns BGR to BGRA
		cv::cvtColor(screen_pixels(viewport_in_screen_rect), _viewport_pixels, cv::COLOR_BGR2BGRA); // only take pixels of viewport and add alpha channel
		
		// Create gray version of viewport pixels
		core::opencv::BGRA2Y(_viewport_pixels, _viewport_pixels_gray);
	}

	const cv::Mat LogImage::get_layer_pixels(const cv::Mat layer_view_mask) const
	{
		cv::Mat layer_pixels = cv::Mat::zeros(_viewport_pixels.size(), _viewport_pixels.type()); // initialize empty image
		core::opencv::blend(_viewport_pixels, layer_pixels, layer_view_mask, layer_pixels); // blend pixels onto it
		return layer_pixels;
	}
}
