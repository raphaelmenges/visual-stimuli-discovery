#include "EdgeChangeFraction.hpp"

#include <opencv2/opencv.hpp>

namespace feature
{
	EdgeChangeFraction::EdgeChangeFraction(
		std::shared_ptr<const cv::Mat> a,
		std::shared_ptr<const cv::Mat> b)
		:
		Interface(a, b)
	{
		const int canny_lower_threshold = 64;
		const int dilation_kernel_size = 2; // makes a diameter of 5

		// BGRA to Y
		cv::Mat gray_a, gray_b;
		core::opencv::BGRA2Y(*a, gray_a);
		core::opencv::BGRA2Y(*b, gray_b);

		// Canny
		cv::Mat edges_a;
		cv::Mat edges_b;
		cv::Canny(gray_a, edges_a, canny_lower_threshold, 3 * canny_lower_threshold); // ratio recommended: https://docs.opencv.org/2.4/doc/tutorials/imgproc/imgtrans/canny_detector/canny_detector.html
		cv::Canny(gray_b, edges_b, canny_lower_threshold, 3 * canny_lower_threshold);

		// Get alpha of input images
		cv::Mat alpha = cv::Mat::zeros(a->size(), CV_8UC1);
		int from_to[] = { 3, 0 };
		cv::mixChannels(a.get(), 1, &alpha, 1, from_to, 1);

		// Erode alpha a bit
		int ero_kernel_size = 1;
		cv::Mat ero_kernel = cv::getStructuringElement(
			cv::MORPH_RECT,
			cv::Size(2 * ero_kernel_size + 1, 2 * ero_kernel_size + 1),
			cv::Point(ero_kernel_size, ero_kernel_size));
		cv::erode(alpha, alpha, ero_kernel, cv::Point(-1, -1), 1, 0, 0); // black border assumed

		// Mask edges at "layer edges"
		edges_a = edges_a & alpha;
		edges_b = edges_b & alpha;

		// Dilate edges
		cv::Mat dil_kernel = cv::getStructuringElement(
			cv::MORPH_RECT,
			cv::Size(2 * dilation_kernel_size + 1, 2 * dilation_kernel_size + 1),
			cv::Point(dilation_kernel_size, dilation_kernel_size));
		cv::Mat dil_a;
		cv::Mat dil_b;
		cv::dilate(edges_a, dil_a, dil_kernel);
		cv::dilate(edges_b, dil_b, dil_kernel);

		// Filter for incoming and outgoing edges
		cv::Mat inv_dil_a = 255 - dil_a;
		cv::Mat inv_dil_b = 255 - dil_b;
		cv::Mat incoming = (inv_dil_a & edges_b); // incoming edges, introduced by b
		cv::Mat outgoing = (inv_dil_b & edges_a); // outgoing edges, taken away at b

		// Count pixels
		int edges_count_a = cv::countNonZero(edges_a); // edge count in a
		int edges_count_b = cv::countNonZero(edges_b); // edge count in b
		int incoming_count = cv::countNonZero(incoming); // count of incoming edges
		int outgoing_count = cv::countNonZero(outgoing); // count of outgoing edges

		// Function for safe division
		std::function<double(int, int)> safe_div = [](int x, int y) {if (y == 0) { return 0.0; } else { return (double)x / (double)y; } };

		// Calculate edge change ratio
		// double ecr = std::max(safe_div(outgoing_count, edges_count_a), safe_div(incoming_count, edges_count_b));

		// Calculate edge change fraction
		double ecf = std::max(outgoing_count, incoming_count);

		// Store feature
		_features["edge_change_fraction"] = ecf;

		/*
		// Manual debugging
		cv::imshow("Gray A", gray_a);
		cv::imshow("Gray B", gray_b);
		cv::imshow("Edges A", edges_a);
		cv::imshow("Edges B", edges_b);
		*/
	}
}