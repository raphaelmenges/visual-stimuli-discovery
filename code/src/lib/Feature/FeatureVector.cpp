#include "FeatureVector.hpp"
#include <Descriptor/OCR.hpp>
#include <Feature/Histogram.hpp>
#include <Feature/BagOfWords.hpp>
#include <Feature/NGrams.hpp>
#include <Feature/PixelDiff.hpp>
#include <Feature/EdgeChangeFraction.hpp>
#include <Feature/MSSIM.hpp>
#include <Feature/PSNR.hpp>
#include <Feature/SiftMatch.hpp>
#include <Feature/OpticalFlow.hpp>
#include <opencv2/opencv.hpp>
#include <chrono>

const int MARGIN_KERNEL_SIZE = core::mt::get_config_value(1, { "feature_vector", "margin_kernel_size" });
const bool ENABLE_HISTOGRAM = core::mt::get_config_value(true, { "feature_vector", "enable", "histogram" });
const bool ENABLE_PIXEL_DIFF = core::mt::get_config_value(true, { "feature_vector", "enable", "pixel_diff" });
const bool ENABLE_EDGE_CHANGE_RATIO = core::mt::get_config_value(true, { "feature_vector", "enable", "edge_change_ratio" });
const bool ENABLE_MSSIM = core::mt::get_config_value(true, { "feature_vector", "enable", "mssim" });
const bool ENABLE_PSNR = core::mt::get_config_value(true, { "feature_vector", "enable", "psnr" });
const bool ENABLE_SIFT = core::mt::get_config_value(true, { "feature_vector", "enable", "sift" });
const bool ENABLE_BAG_OF_WORDS = core::mt::get_config_value(true, { "feature_vector", "enable", "bag_of_words" });
const bool ENABLE_N_GRAMS = core::mt::get_config_value(true, { "feature_vector", "enable", "n_grams" });
const bool ENABLE_OPTICAL_FLOW = core::mt::get_config_value(true, { "feature_vector", "enable", "optical_flow" });

namespace feature
{
	FeatureVector::FeatureVector(
		std::shared_ptr<const cv::Mat> a,
		std::shared_ptr<const cv::Mat> b)
	{
		// Introduce margin
		auto a_margin = std::make_shared<cv::Mat>();
		auto b_margin = std::make_shared<cv::Mat>();
		core::opencv::erodeAlpha(*a, *a_margin, MARGIN_KERNEL_SIZE);
		core::opencv::erodeAlpha(*b, *b_margin, MARGIN_KERNEL_SIZE);

		// Crop to get rid of eroded border (TODO: this might produce empty matrices. Catch that case!)
		core::opencv::overlap_and_crop(*a_margin, *b_margin, *a_margin, *b_margin);

		// Compose lambda to measure timings
		auto time = std::chrono::steady_clock::now();
		std::map<std::string, int> times;
		auto take_and_reset_time = std::function<void(std::string)>([&time, &times](std::string name)
		{
			auto new_time = std::chrono::steady_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(new_time - time);
			time = new_time;
			name += " [ms]";
			times[name] = (int)duration.count();
		});
		auto reset_time = std::function<void()>([&time]()
		{
			time = std::chrono::steady_clock::now();
		});

		/////////////////////////////////////////////////
		/// Standard input images
		/////////////////////////////////////////////////

		{
			// Histogram
			if(ENABLE_HISTOGRAM)
			{
				auto sp_histogram_a = std::make_shared<descriptor::Histogram>(a_margin);
				auto sp_histogram_b = std::make_shared<descriptor::Histogram>(b_margin);
				take_and_reset_time("histogram_descriptors");
				auto histogram = (feature::Histogram(sp_histogram_a, sp_histogram_b)).get();
				_features.insert(histogram.begin(), histogram.end());
				take_and_reset_time("histogram_features");
			}
			
			// Pixel diff
			if(ENABLE_PIXEL_DIFF)
			{
				auto pixel_diff = (feature::PixelDiff(a_margin, b_margin)).get();
				_features.insert(pixel_diff.begin(), pixel_diff.end());
				take_and_reset_time("pixel_diff_features");
			}
			
			// Edge change ratio
			if(ENABLE_EDGE_CHANGE_RATIO)
			{
				auto edge_change_ratio = (feature::EdgeChangeFraction(a_margin, b_margin)).get();
				_features.insert(edge_change_ratio.begin(), edge_change_ratio.end());
				take_and_reset_time("edge_change_ratio_features");
			}
			
			// MSSIM
			if(ENABLE_MSSIM)
			{
				auto mssim = (feature::MSSIM(a_margin, b_margin)).get();
				_features.insert(mssim.begin(), mssim.end());
				take_and_reset_time("mssim_features");
			}
			
			// PSNR
			if(ENABLE_PSNR)
			{
				auto psnr = (feature::PSNR(a_margin, b_margin)).get();
				_features.insert(psnr.begin(), psnr.end());
				take_and_reset_time("psnr_features");
			}
			
			// SIFT
			if(ENABLE_SIFT)
			{
				auto sift_match = (feature::SiftMatch(a_margin, b_margin)).get();
				_features.insert(sift_match.begin(), sift_match.end());
				take_and_reset_time("sift_match_features");
			}
			
			// OCR
			if(ENABLE_BAG_OF_WORDS || ENABLE_N_GRAMS)
			{
				auto sp_prev_ocr = std::make_shared<descriptor::OCR>(a_margin);
				auto sp_ocr = std::make_shared<descriptor::OCR>(b_margin);
				take_and_reset_time("ocr_descriptors");
				
				// Bag of words
				if(ENABLE_BAG_OF_WORDS)
				{
					auto bag_of_words = (feature::BagOfWords(sp_prev_ocr->get_words(), sp_ocr->get_words())).get();
					_features.insert(bag_of_words.begin(), bag_of_words.end());
					take_and_reset_time("bag_of_words_features");
				}
				
				// N-grams
				if(ENABLE_N_GRAMS)
				{
					auto n_grams = (feature::NGrams(sp_prev_ocr->get_words(), sp_ocr->get_words())).get();
					_features.insert(n_grams.begin(), n_grams.end());
					take_and_reset_time("n_grams_features");
				}
			}
			
			// Optical flow
			if(ENABLE_OPTICAL_FLOW)
			{
				auto optical_flow = (feature::OpticalFlow(a_margin, b_margin)).get();
				_features.insert(optical_flow.begin(), optical_flow.end());
				take_and_reset_time("optical_flow_features");
			}
		}

		/*

		/////////////////////////////////////////////////
		/// Dilated input images
		/////////////////////////////////////////////////

		std::map<std::string, double> dilate_features;
		{
			const int kernel_size = 1;

			// Create kernel
			cv::Mat kernel = cv::getStructuringElement(
				cv::MORPH_RECT,
				cv::Size(2 * kernel_size + 1, 2 * kernel_size + 1),
				cv::Point(kernel_size, kernel_size));

			std::map<std::string, double> features;

			// Increase bright areas, get rid of dark underlinings
			auto sp_dil_a = std::make_shared<cv::Mat>();
			auto sp_dil_b = std::make_shared<cv::Mat>();
			cv::dilate(*a_margin, *sp_dil_a, kernel);
			cv::dilate(*b_margin, *sp_dil_b, kernel);
			cv::erode(*sp_dil_a, *sp_dil_a, kernel);
			cv::erode(*sp_dil_b, *sp_dil_b, kernel);

			// Create descriptors
			auto sp_histogram_dil_a = std::make_shared<descriptor::Histogram>(sp_dil_a);
			auto sp_histogram_dil_b = std::make_shared<descriptor::Histogram>(sp_dil_b);

			// Compute features
			auto histogram = (feature::Histogram(sp_histogram_dil_a, sp_histogram_dil_b)).get();
			features.insert(histogram.begin(), histogram.end());
			auto pixel_diff = (feature::PixelDiff(sp_dil_a, sp_dil_b)).get();
			features.insert(pixel_diff.begin(), pixel_diff.end());
			auto edge_change_ratio = (feature::EdgeChangeFraction(sp_dil_a, sp_dil_b)).get();
			features.insert(edge_change_ratio.begin(), edge_change_ratio.end());

			// Add suffix to all dilate features
			std::for_each(features.begin(), features.end(), [&](std::pair<const std::string, double>& pair) { dilate_features[pair.first + "_d1"] = pair.second; });
		}
		_features.insert(dilate_features.begin(), dilate_features.end());

		/////////////////////////////////////////////////
		/// Eroded input images
		/////////////////////////////////////////////////

		std::map<std::string, double> erode_features;
		{
			const int kernel_size = 1;

			// Create kernel
			cv::Mat kernel = cv::getStructuringElement(
				cv::MORPH_RECT,
				cv::Size(2 * kernel_size + 1, 2 * kernel_size + 1),
				cv::Point(kernel_size, kernel_size));

			std::map<std::string, double> features;

			// Increase dark areas, get rid of bright underlinings
			auto sp_ero_a = std::make_shared<cv::Mat>();
			auto sp_ero_b = std::make_shared<cv::Mat>();
			cv::erode(*a_margin, *sp_ero_a, kernel);
			cv::erode(*b_margin, *sp_ero_b, kernel);
			cv::dilate(*sp_ero_a, *sp_ero_a, kernel);
			cv::dilate(*sp_ero_b, *sp_ero_b, kernel);

			// Create descriptors
			auto sp_histogram_ero_a = std::make_shared<descriptor::Histogram>(sp_ero_a);
			auto sp_histogram_ero_b = std::make_shared<descriptor::Histogram>(sp_ero_b);

			// Compute features
			auto histogram = (feature::Histogram(sp_histogram_ero_a, sp_histogram_ero_b)).get();
			features.insert(histogram.begin(), histogram.end());
			auto pixel_diff = (feature::PixelDiff(sp_ero_a, sp_ero_b)).get();
			features.insert(pixel_diff.begin(), pixel_diff.end());
			auto edge_change_ratio = (feature::EdgeChangeFraction(sp_ero_a, sp_ero_b)).get();
			features.insert(edge_change_ratio.begin(), edge_change_ratio.end());

			// Add suffix to all erode features
			std::for_each(features.begin(), features.end(), [&](std::pair<const std::string, double>& pair) { erode_features[pair.first + "_e1"] = pair.second; });
		}
		_features.insert(erode_features.begin(), erode_features.end());

		*/

		// Store timings
		_times = times;
	}
}
