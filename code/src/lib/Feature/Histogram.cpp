#include "Histogram.hpp"

#include <opencv2/opencv.hpp>

namespace feature
{
	// TODO: consider following means of comparison
	// CV_COMP_CORREL = 0,
	// CV_COMP_CHISQR = 1,
	// CV_COMP_INTERSECT = 2,
	// CV_COMP_BHATTACHARYYA = 3,

	Histogram::Histogram(
		std::shared_ptr<const descriptor::Histogram> a,
		std::shared_ptr<const descriptor::Histogram> b)
		:
		Interface(a, b)
	{
		_features["histogram_blue_correl"] = (double)cv::compareHist(*(a->get_blue_hist()), *(b->get_blue_hist()), cv::HISTCMP_CORREL);
		_features["histogram_green_correl"] = (double)cv::compareHist(*(a->get_green_hist()), *(b->get_green_hist()), cv::HISTCMP_CORREL);
		_features["histogram_red_correl"] = (double)cv::compareHist(*(a->get_red_hist()), *(b->get_red_hist()), cv::HISTCMP_CORREL);
		_features["histogram_hue_correl"] = (double)cv::compareHist(*(a->get_hue_hist()), *(b->get_hue_hist()), cv::HISTCMP_CORREL);
		_features["histogram_saturation_correl"] = (double)cv::compareHist(*(a->get_saturation_hist()), *(b->get_saturation_hist()), cv::HISTCMP_CORREL);
		_features["histogram_lightness_correl"] = (double)cv::compareHist(*(a->get_lightness_hist()), *(b->get_lightness_hist()), cv::HISTCMP_CORREL);
		_features["histogram_gray_correl"] = (double)cv::compareHist(*(a->get_gray_hist()), *(b->get_gray_hist()), cv::HISTCMP_CORREL);
	}
}
