//!  Classifier for visual change. Usage is multithread-safe
/*!
Interface for multi-threaded tunings that are applied on log dates.
*/

#pragma once

#include <Feature/FeatureVector.hpp>
#include <Learn/RandomForest.hpp>
#include <Data/Dataset.hpp>
#include <memory>
#include <mutex>

namespace core
{
	class VisualChangeClassifier
	{
	public:

		// Constructor
		VisualChangeClassifier(
			std::string features_file_path,
			std::string labels_file_path);

		// Classify a feature vector (normalization must be done by caller)
		std::shared_ptr<const Eigen::VectorXd>
			classify(std::shared_ptr<const data::Dataset> sp_dataset) const;

		// Get min-max of trainig data to normalize dataset accordingly
		std::map<std::string, std::pair<double, double> > get_min_max() const
		{
			return _min_max;
		}

	private:

		// Members
		std::unique_ptr<learn::RandomForest> _up_forest = nullptr;
		std::map<std::string, std::pair<double, double> > _min_max;
		mutable std::mutex _classify_mutex;
	};
}
