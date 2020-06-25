#include "Histogram.hpp"

#include <Core/Core.hpp>
#include <opencv2/opencv.hpp>

namespace descriptor
{
	Histogram::Histogram(std::shared_ptr<const cv::Mat> sp_image)
	{
		// Split color planes of input image (assuming BGRA)
		std::vector<cv::Mat> bgra_planes;
		cv::split(*sp_image, bgra_planes);

		// Create BGR histograms
		int hist_size = 16; // number of bins
		float range[] = { 0, 256 };
		const float* hist_range = { range };
		bool uniform = true, accumulate = false;
		cv::calcHist(&bgra_planes[0], 1, 0, bgra_planes[3], *_sp_blue_hist, 1, &hist_size, &hist_range, uniform, accumulate);
		cv::calcHist(&bgra_planes[1], 1, 0, bgra_planes[3], *_sp_green_hist, 1, &hist_size, &hist_range, uniform, accumulate);
		cv::calcHist(&bgra_planes[2], 1, 0, bgra_planes[3], *_sp_red_hist, 1, &hist_size, &hist_range, uniform, accumulate);

		// Create HSL histograms (reuse alpha from bgra planes as mask)
		cv::Mat hls_mat;
		std::vector<cv::Mat> bgr_mats = {
			bgra_planes.at(0),
			bgra_planes.at(1),
			bgra_planes.at(2)
		};
		cv::merge(bgr_mats, hls_mat);
		cv::cvtColor(hls_mat, hls_mat, cv::COLOR_BGR2HLS);
		std::vector<cv::Mat> hls_planes;
		cv::split(hls_mat, hls_planes);
		cv::calcHist(&hls_planes[0], 1, 0, bgra_planes[3], *_sp_hue_hist, 1, &hist_size, &hist_range, uniform, accumulate);
		cv::calcHist(&hls_planes[1], 1, 0, bgra_planes[3], *_sp_lightness_hist, 1, &hist_size, &hist_range, uniform, accumulate);
		cv::calcHist(&hls_planes[2], 1, 0, bgra_planes[3], *_sp_saturation_hist, 1, &hist_size, &hist_range, uniform, accumulate);

		// Create grayscale histogram
		cv::Mat gray_plane;
		core::opencv::BGRA2Y(*sp_image, gray_plane);
		cv::calcHist(&gray_plane, 1, 0, bgra_planes[3], *_sp_gray_hist, 1, &hist_size, &hist_range, uniform, accumulate);
	}
}
