#include "OpticalFlow.hpp"

#include <opencv2/video/tracking.hpp>

namespace feature
{
	OpticalFlow::OpticalFlow(
		std::shared_ptr<const cv::Mat> a,
		std::shared_ptr<const cv::Mat> b)
		:
		Interface(a, b)
	{
		// BGRA to Y
		cv::Mat gray_a, gray_b;
		core::opencv::BGRA2Y(*a, gray_a);
		core::opencv::BGRA2Y(*b, gray_b);

		// Calculate u and v flow
		cv::Mat flow_uv;
		cv::calcOpticalFlowFarneback(
			gray_a,
			gray_b,
			flow_uv, // output, 2D-floating point matrix
			0.4, // pyr_scale
			3, // levels
			10, // winsize
			2, // iterations
			5, // poly_n
			1.1, // poly_sigma
			0); // flags

		// Retrieve magnitude and angle of uv flow
		std::vector<cv::Mat> uv_planes;
		cv::split(flow_uv, uv_planes);
		cv::Mat magnitude, angle;
		cv::cartToPolar(uv_planes[0], uv_planes[1], magnitude, angle, true); // angle in degrees

		// Feature values of magnitude
		std::vector<float> magnitude_vec(magnitude.begin<float>(), magnitude.end<float>());
		auto magnitude_minmax = std::minmax_element(magnitude_vec.begin(), magnitude_vec.end());
		cv::Mat magnitude_mean, magnitude_stddev;
		cv::meanStdDev(magnitude, magnitude_mean, magnitude_stddev);

		// Store features
		_features["optical_flow_magnitude_min"] = *(magnitude_minmax.first);
		_features["optical_flow_magnitude_max"] = *(magnitude_minmax.second);
		_features["optical_flow_magnitude_mean"] = magnitude_mean.data[0];
		_features["optical_flow_magnitude_stddev"] = magnitude_stddev.data[0];
		
		// Feature values of angle
		std::vector<float> angle_vec(angle.begin<float>(), angle.end<float>());
		auto angle_minmax = std::minmax_element(angle_vec.begin(), angle_vec.end());
		cv::Mat angle_mean, angle_stddev;
		cv::meanStdDev(angle, angle_mean, angle_stddev);

		// Store features
		_features["optical_flow_angle_min"] = *(angle_minmax.first);
		_features["optical_flow_angle_max"] = *(angle_minmax.second);
		_features["optical_flow_angle_mean"] = angle_mean.data[0];
		_features["optical_flow_angle_stddev"] = angle_stddev.data[0];
	}
}
