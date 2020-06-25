#include "PixelDiff.hpp"

#include <opencv2/opencv.hpp>

namespace feature
{
	PixelDiff::PixelDiff(
		std::shared_ptr<const cv::Mat> a,
		std::shared_ptr<const cv::Mat> b)
		:
		Interface(a, b)
	{
		// Color
		{
			core::long64 diff_acc_bgr = 0;
			core::long64 diff_acc_b = 0;
			core::long64 diff_acc_g = 0;
			core::long64 diff_acc_r = 0;
			core::long64 diff_count_bgr = 0;
			core::long64 diff_count_b = 0;
			core::long64 diff_count_g = 0;
			core::long64 diff_count_r = 0;
			core::long64 count = 0;

			// Go over pixels of both matrices
			for (unsigned int x = 0; x < (unsigned int)a->cols; ++x)
			{
				for (unsigned int y = 0; y < (unsigned int)a->rows; ++y)
				{
					// Retrieve pixel values
					const auto& current_pixel = a->at<cv::Vec4b>(y, x);
					const auto& potential_pixel = b->at<cv::Vec4b>(y, x);

					// First, check the alpha values
					if (current_pixel[3] <= 0 || potential_pixel[3] <= 0) { continue; } // empty pixel, do not compare these

					// Retrieve differences (0..255 range)
					core::long64 diff_b = std::abs(current_pixel[0] - potential_pixel[0]);
					core::long64 diff_g = std::abs(current_pixel[1] - potential_pixel[1]);
					core::long64 diff_r = std::abs(current_pixel[2] - potential_pixel[2]);
					core::long64 diff_bgr = diff_b + diff_g + diff_r;
					
					// Accumulate differences
					diff_acc_b += diff_b;
					diff_acc_g += diff_g;
					diff_acc_r += diff_r;
					diff_acc_bgr += diff_bgr;

					// Remember that there was a difference
					if (diff_b > 0.0)
					{
						++diff_count_b;
					}
					if (diff_g > 0.0)
					{
						++diff_count_g;
					}
					if (diff_r > 0.0)
					{
						++diff_count_r;
					}
					if (diff_bgr > 0.0)
					{
						++diff_count_bgr;
					}

					++count;
				}
			}

			/*
			if (count <= 0) { count = 1; } // prohibit division by zero
			_features["pixel_diff_acc_bgr"] = diff_acc / (double)count;
			_features["pixel_diff_count_bgr"] = (double)diff_count / (double)count;
			*/
			_features["pixel_diff_acc_bgr"] = (double) diff_acc_bgr;
			_features["pixel_diff_count_bgr"] = (double) diff_count_bgr;
			_features["pixel_diff_acc_b"] = (double) diff_acc_b;
			_features["pixel_diff_count_b"] = (double) diff_count_b;
			_features["pixel_diff_acc_g"] = (double) diff_acc_g;
			_features["pixel_diff_count_g"] = (double) diff_count_g;
			_features["pixel_diff_acc_r"] = (double) diff_acc_r;
			_features["pixel_diff_count_r"] = (double) diff_count_r;

		} // end color

		// Gray
		{
			core::long64 diff_acc = 0;
			core::long64 diff_count = 0;
			core::long64 count = 0;

			// BGRA to Y
			cv::Mat gray_a, gray_b;
			core::opencv::BGRA2Y(*a, gray_a);
			core::opencv::BGRA2Y(*b, gray_b);

			// Go over pixels of both matrices
			for (unsigned int x = 0; x < (unsigned int)gray_a.cols; ++x)
			{
				for (unsigned int y = 0; y < (unsigned int)gray_a.rows; ++y)
				{
					// Retrieve pixel values
					const auto& current_pixel = gray_a.at<uchar>(y, x);
					const auto& potential_pixel = gray_b.at<uchar>(y, x);
					const auto& current_pixel_bgra = a->at<cv::Vec4b>(y, x);
					const auto& potential_pixel_bgra = b->at<cv::Vec4b>(y, x);

					// First, check the alpha values
					if (current_pixel_bgra[3] <= 0 || potential_pixel_bgra[3] <= 0) { continue; } // empty pixel, do not compare these

					// Add raw differences(0..255 range)
					core::long64 diff = std::abs(current_pixel - potential_pixel);
					diff_acc += diff;

					// Remember that there was a difference
					if (diff > 0.0)
					{
						++diff_count;
					}

					++count;
				}
			}

			/*
			if (count <= 0) { count = 1; } // prohibit division by zero
			_features["pixel_diff_acc_gray"] = diff_acc / (double)count;
			_features["pixel_diff_count_gray"] = (double)diff_count / count;
			*/
			_features["pixel_diff_acc_gray"] = (double) diff_acc;
			_features["pixel_diff_count_gray"] = (double) diff_count;

		} // end gray

		// HLS
		{
			core::long64 diff_acc_hue = 0;
			core::long64 diff_count_hue = 0;
			core::long64 diff_acc_saturation = 0;
			core::long64 diff_count_saturation = 0;
			core::long64 diff_acc_lightness = 0;
			core::long64 diff_count_lightness = 0;
			core::long64 count = 0;

			// Split color planes of input images (assuming BGRA)
			std::vector<cv::Mat> bgra_planes_a;
			std::vector<cv::Mat> bgra_planes_b;
			cv::split(*a, bgra_planes_a);
			cv::split(*b, bgra_planes_b);

			// Create matrices in HLS space (reuse alpha from bgra planes as mask)
			cv::Mat hls_a, hls_b;
			std::vector<cv::Mat> bgr_mats_a = {
				bgra_planes_a.at(0),
				bgra_planes_a.at(1),
				bgra_planes_a.at(2)
			};
			std::vector<cv::Mat> bgr_mats_b = {
				bgra_planes_b.at(0),
				bgra_planes_b.at(1),
				bgra_planes_b.at(2)
			};
			cv::merge(bgr_mats_a, hls_a);
			cv::merge(bgr_mats_b, hls_b);
			cv::cvtColor(hls_a, hls_a, cv::COLOR_BGR2HLS);
			cv::cvtColor(hls_b, hls_b, cv::COLOR_BGR2HLS);

			// Go over pixels of both matrices
			for (unsigned int x = 0; x < (unsigned int)a->cols; ++x)
			{
				for (unsigned int y = 0; y < (unsigned int)a->rows; ++y)
				{
					// Retrieve pixel values
					const auto& current_pixel = hls_a.at<cv::Vec3b>(y, x);
					const auto& potential_pixel = hls_b.at<cv::Vec3b>(y, x);
					const auto& current_pixel_bgra = a->at<cv::Vec4b>(y, x);
					const auto& potential_pixel_bgra = b->at<cv::Vec4b>(y, x);

					// First, check the alpha values
					if (current_pixel_bgra[3] <= 0 || potential_pixel_bgra[3] <= 0) { continue; } // empty pixel, do not compare these

					// Add raw differences (0..180 range, according to documentation)
					core::long64 diff_hue = std::abs(current_pixel[0] - potential_pixel[0]); // hue is in first channel
					diff_acc_hue += diff_hue;

					// Remember that there was a difference
					if (diff_hue > 0.0)
					{
						++diff_count_hue;
					}
					
					// Add raw differences (0..255)
					core::long64 diff_saturation = std::abs(current_pixel[2] - potential_pixel[2]); // saturation is in third channel
					diff_acc_saturation += diff_saturation;

					// Remember that there was a difference
					if (diff_saturation > 0.0)
					{
						++diff_count_saturation;
					}

					// Add raw differences (0..255)
					core::long64 diff_lightness = std::abs(current_pixel[1] - potential_pixel[1]); // lightness is in second channel
					diff_acc_lightness += diff_lightness;

					// Remember that there was a difference
					if (diff_lightness > 0.0)
					{
						++diff_count_lightness;
					}

					++count;
				}
			}

			/*
			if (count <= 0) { count = 1; } // prohibit division by zero
			_features["pixel_diff_hue_acc"] = diff_hue_acc / (double)count;
			_features["pixel_diff_hue_count"] = (double)diff_hue_count / (double)count;
			_features["pixel_diff_value_acc"] = diff_value_acc / (double)count;
			_features["pixel_diff_value_count"] = (double)diff_value_count / (double)count;
			*/
			_features["pixel_diff_acc_hue"] = (double) diff_acc_hue;
			_features["pixel_diff_count_hue"] = (double) diff_count_hue;
			_features["pixel_diff_acc_saturation"] = (double) diff_acc_saturation;
			_features["pixel_diff_count_saturation"] = (double) diff_count_saturation;
			_features["pixel_diff_acc_lightness"] = (double) diff_acc_lightness;
			_features["pixel_diff_count_lightness"] = (double) diff_count_lightness;

		} // end hue
	}
}
