#include "VisualChangeClassifier.hpp"

#include <Core/Core.hpp>
#include <Data/Dataset.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace core
{
	VisualChangeClassifier::VisualChangeClassifier(
		std::string features_file_path,
		std::string labels_file_path)
	{
		core::mt::log_info("# Training of Visual Change Classifier");

		// Load dataset to train random forest
		auto sp_train_dataset = std::make_shared<data::Dataset>(features_file_path, labels_file_path);

		// Load an image to compute a feature vector with the currently enabled features
		auto sp_img_a = std::make_shared<const cv::Mat>(cv::imread(core::mt::res_path() + "misc/digg_1.png", -1)); // load with alpha
		auto sp_img_b = std::make_shared<const cv::Mat>(cv::imread(core::mt::res_path() + "misc/digg_2.png", -1)); // load with alpha

		// Compute feature vector and filter dataset to include only features that will be actually computed
		auto feature_vector = feature::FeatureVector(sp_img_a, sp_img_b);

		// Only take features from training dataset which are later computed
		sp_train_dataset->filter_features(feature_vector.get_names());
		
		// Print considered features to console
		core::mt::log_info("## List of considered features");
		auto feature_names = sp_train_dataset->get_feature_names();
		for(const auto& r_name : feature_names)
		{
			core::mt::log_info("- ", r_name);
		}

		// Store min-max to normalize later data according to training data
		_min_max = sp_train_dataset->get_min_max();

		// Train random forest
		sp_train_dataset->normalize();
		_up_forest = std::unique_ptr<learn::RandomForest>(new learn::RandomForest(sp_train_dataset));
	}

	std::shared_ptr<const Eigen::VectorXd>
		VisualChangeClassifier::classify(std::shared_ptr<const data::Dataset> sp_dataset) const
	{
		std::lock_guard<std::mutex> guard(_classify_mutex);
		return _up_forest->classify(sp_dataset);
	}
}
