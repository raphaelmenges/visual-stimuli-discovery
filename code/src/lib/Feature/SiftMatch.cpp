#include "SiftMatch.hpp"

#include <opencv2/opencv.hpp>
#include <opencv2/xfeatures2d.hpp>

namespace feature
{
	SiftMatch::SiftMatch(
		std::shared_ptr<const cv::Mat> a,
		std::shared_ptr<const cv::Mat> b)
		:
		Interface(a, b)
	{
		// Prepare visual debug output
		/*
		VD(
		std::shared_ptr<core::visual_debug::Datum> sp_sift_match_datum = nullptr;
		if (sp_datum)
		{
			sp_sift_match_datum = vd_datum("Sift Match Feature");
			sp_datum->add(sp_sift_match_datum);
		})
		*/

		// Constants (it seems like that on smaller images, the number of matches is limited by the size of the image)
		const int feature_count = 500;
		// const int max_intra_match_threshold = 32;

		// BGRA to Y
		cv::Mat gray_a, gray_b;
		core::opencv::BGRA2Y(*a, gray_a);
		core::opencv::BGRA2Y(*b, gray_b);

		// Extract alpha
		cv::Mat alpha = cv::Mat::zeros(a->size(), CV_8UC1);
		int from_to[] = { 3, 0 };
		cv::mixChannels(a.get(), 1, &alpha, 1, from_to, 1);

		// Get SIFT features from both images
		auto detector = cv::xfeatures2d::SIFT::create(feature_count);
		std::vector<cv::KeyPoint> keypoints_a, keypoints_b;
		cv::Mat descriptors_a, descriptors_b;
		detector->detectAndCompute(gray_a, alpha, keypoints_a, descriptors_a);
		detector->detectAndCompute(gray_b, alpha, keypoints_b, descriptors_b);

		/*

		// Below code would implicate a different number of keypoints for each observation. Thus, a kind of normalization would be required

		// Compute intra similarity
		auto intra_matcher = cv::BFMatcher::create(cv::NORM_L2);
		std::vector<std::vector<cv::DMatch> > intra_matches_a, intra_matches_b;
		intra_matcher->knnMatch(descriptors_a, descriptors_a, intra_matches_a, 2); // k = 1 would only output similarity with itself
		intra_matcher->knnMatch(descriptors_b, descriptors_b, intra_matches_b, 2); // k = 1 would only output similarity with itself

		// Throw away keypoints which descriptors are matching too good (similar within image)
		{
			std::set<int> to_delete;
			for (const auto& r_match : intra_matches_a)
			{
				for (const auto& r_inner_match : r_match)
				{
					if ((r_inner_match.queryIdx != r_inner_match.trainIdx) && r_inner_match.distance < max_intra_match_threshold)
					{
						to_delete.emplace(r_inner_match.queryIdx);
						to_delete.emplace(r_inner_match.trainIdx);
					}
				}
			}

			// Remove all too good keypoints (back to front)
			for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it)
			{
				keypoints_a.erase(keypoints_a.begin() + *it);
			}
		}
		{
			std::set<int> to_delete;
			for (const auto& r_match : intra_matches_b)
			{
				for (const auto& r_inner_match : r_match)
				{
					if ((r_inner_match.queryIdx != r_inner_match.trainIdx) && r_inner_match.distance < max_intra_match_threshold)
					{
						to_delete.emplace(r_inner_match.queryIdx);
						to_delete.emplace(r_inner_match.trainIdx);
					}
				}
			}

			// Remove all too good keypoints (back to front)
			for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it)
			{
				keypoints_b.erase(keypoints_b.begin() + *it);
			}
		}

		// Recompute descriptors
		descriptors_a = cv::Mat();
		descriptors_b = cv::Mat();
		auto detector_a = cv::xfeatures2d::SIFT::create((int)keypoints_a.size());
		auto detector_b = cv::xfeatures2d::SIFT::create((int)keypoints_b.size());
		detector_a->compute(gray_a, keypoints_a, descriptors_a);
		detector_a->compute(gray_b, keypoints_b, descriptors_b);

		*/
		std::vector<cv::DMatch> matches;
		if (!descriptors_a.empty() && !descriptors_b.empty())
		{
			// Match the keypoints from both images
			auto matcher = cv::BFMatcher::create(cv::NORM_L2); // Euclidean distance to match sift descriptors
			matcher->match(descriptors_a, descriptors_b, matches);
		}

		/*

		// Manual debugging
		core::mt::log_info("Keypoints A: ", keypoints_a.size());
		core::mt::log_info("Keypoints B: ", keypoints_b.size());
		core::mt::log_info("Descriptors A: ", descriptors_a.size());
		core::mt::log_info("Descriptors A: ", descriptors_b.size());
		core::mt::log_info("Matches A: ", matches.size());

		cv::drawKeypoints(gray_a, keypoints_a, gray_a, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
		cv::drawKeypoints(gray_b, keypoints_b, gray_b, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
		cv::imshow("Gray A", gray_a);
		cv::imshow("Gray B", gray_b);
		cv::waitKey(0);

		cv::Mat matchMat;
		cv::drawMatches(gray_a, keypoints_a, gray_b, keypoints_b, matches, matchMat);
		cv::imshow("Matches", matchMat);
		cv::waitKey(0);

		*/

		// Sort matches by similarity and limit number of matches
		std::sort(matches.begin(), matches.end()); // sort predicate is given by OpenCV
		matches.resize(std::min(feature_count, (int)matches.size()));

		// Go through matched data (quick look into data tells me that to-be-expected max is 512)
		std::vector<double> descriptor_distances;
		int match_count = 0;
		int match_count_0 = 0;
		int match_count_4 = 0;
		int match_count_16 = 0;
		int match_count_64 = 0;
		int match_count_256 = 0;
		int match_count_512 = 0;
		int spatial_match_count = 0;
		for (int i = 0; i < (int)matches.size(); i++)
		{
			const auto& r_match = matches.at(i);

			// Store descriptor distance
			descriptor_distances.push_back(r_match.distance);

			// Check spatial distance of matched descriptors
			int index_a = r_match.queryIdx;
			int index_b = r_match.trainIdx;
			bool spatial_close = core::math::euclidean_dist(keypoints_a.at(index_a).pt, keypoints_b.at(index_b).pt) <= 3.f;

			// Count
			match_count++;
			if(spatial_close) { spatial_match_count++; }
			if (r_match.distance <= 0) { match_count_0++; }
			if (r_match.distance <= 4) { match_count_4++; }
			if (r_match.distance <= 16) { match_count_16++; }
			if (r_match.distance <= 64) { match_count_64++; }
			if (r_match.distance <= 256) { match_count_256++; }
			if (r_match.distance <= 512) { match_count_512++; }
		}

		// Initialize output
		double norm_match_count = 0.0;
		double norm_match_count_0 = 0.0;
		double norm_match_count_4 = 0.0;
		double norm_match_count_16 = 0.0;
		double norm_match_count_64 = 0.0;
		double norm_match_count_256 = 0.0;
		double norm_match_count_512 = 0.0;
		double norm_spatial_match_count = 0.0;
		double sift_match_distance_min = 0.0;
		double sift_match_distance_max = 0.0;
		double sift_match_distance_mean = 0.0;
		double sift_match_distance_stddev = 0.0;

		// Normalize counts by number of keypoints (image might be not big enough to fit the number of requested features)
		if (!keypoints_a.empty() && !keypoints_b.empty() && !matches.empty() && !descriptor_distances.empty())
		{
			norm_match_count = (double)match_count / (double)std::max(keypoints_a.size(), keypoints_b.size());
			norm_match_count_0 = (double)match_count_0 / (double)std::max(keypoints_a.size(), keypoints_b.size());
			norm_match_count_4 = (double)match_count_4 / (double)std::max(keypoints_a.size(), keypoints_b.size());
			norm_match_count_16 = (double)match_count_16 / (double)std::max(keypoints_a.size(), keypoints_b.size());
			norm_match_count_64 = (double)match_count_64 / (double)std::max(keypoints_a.size(), keypoints_b.size());
			norm_match_count_256 = (double)match_count_256 / (double)std::max(keypoints_a.size(), keypoints_b.size());
			norm_match_count_512 = (double)match_count_512 / (double)std::max(keypoints_a.size(), keypoints_b.size());
			norm_spatial_match_count = (double)spatial_match_count / (double)std::max(keypoints_a.size(), keypoints_b.size());

			// Global information accross all SIFT features
			auto descriptor_distances_minmax = std::minmax_element(matches.begin(), matches.end());
			cv::Mat descriptor_distances_mean, descriptor_distances_stddev;
			cv::meanStdDev(descriptor_distances, descriptor_distances_mean, descriptor_distances_stddev);

			sift_match_distance_min = descriptor_distances_minmax.first->distance;
			sift_match_distance_max = descriptor_distances_minmax.second->distance;
			sift_match_distance_mean = descriptor_distances_mean.data[0];
			sift_match_distance_stddev = descriptor_distances_stddev.data[0];
		}

		// Store feature values
		_features["sift_match_distance_min"] = sift_match_distance_min;
		_features["sift_match_distance_max"] = sift_match_distance_max;
		_features["sift_match_distance_mean"] = sift_match_distance_mean;
		_features["sift_match_distance_stddev"] = sift_match_distance_stddev;
		_features["sift_match"] = norm_match_count;
		_features["sift_match_0"] = norm_match_count_0;
		_features["sift_match_4"] = norm_match_count_4;
		_features["sift_match_16"] = norm_match_count_16;
		_features["sift_match_64"] = norm_match_count_64;
		_features["sift_match_256"] = norm_match_count_256;
		_features["sift_match_512"] = norm_match_count_512;
		_features["sift_match_spatial"] = norm_spatial_match_count;

		// Print features to visual debug datum
		/*
		VD(
		if (sp_sift_match_datum)
		{
			for (const auto& r_feature : _features)
			{
				sp_sift_match_datum->add(vd_strings(r_feature.first + ": " + std::to_string(r_feature.second)));
			}
		})
		*/
	}
}
