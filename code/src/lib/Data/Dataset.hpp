#pragma once

#include <Eigen/Dense>
#include <set>
#include <vector>
#include <map>

// Note: each row is an observation aka feature vector

namespace data
{
	class Dataset
	{
	public:

		// Constructor takes names of features. Features with other names will be ignored when observation is appended
		Dataset(
			const std::vector<std::string>& r_feature_names,
			double init_value = std::numeric_limits<double>::quiet_NaN());

		// Constructor takes file paths to feature CSV file and label CSV file
		Dataset(
			std::string features_file_path,
			std::string labels_file_path,
			double init_value = std::numeric_limits<double>::quiet_NaN());

		// Constructor takes file path to feature CSV file, only. Labels are initialized with NaN by default
		Dataset(
			std::string features_file_path,
			double init_value = std::numeric_limits<double>::quiet_NaN());

		// Only features contained in the map will be set for the observation, the others remain with the init_value. If no label is provided, NaN is stored as label
		// Labels are expected to be 0..num_classes-1.
		void append_observation(const std::map<std::string, double>& r_features, double label = std::numeric_limits<double>::quiet_NaN());
		
		// Filter features by name. Other features will be discarded. If a name is not found, it is ignored
		void filter_features(const std::vector<std::string>& r_feature_names);

		// Set labels in dataset. Labels are expected to be 0..num_classes-1 and observation-many.
		void set_labels(Eigen::VectorXd labels);

		// Normalize values of feature observations to a range of zero to one
		void normalize();

		// Normalize with given min-max values (e.g., when test dataset should be normalized similar to training dataset)
		void normalize(const std::map<std::string, std::pair<double, double> >& r_min_max);

		// Get min-max values for features. Map consists of feature name as key and min and max as value pair
		std::map<std::string, std::pair<double, double> > get_min_max() const;

		// Get values of feature observations, feature_names defines which features in which order should be provided.
		// Optionally, fill string vector with feature names as provided in the result. E.g., some features requested might be not available and thus not included
		Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
			get_observations_row_wise(const std::vector<std::string>& r_feature_names, std::vector<std::string>* p_result_feature_names = nullptr) const;
		Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>
			get_observations_column_wise(const std::vector<std::string>& r_feature_names, std::vector<std::string>* p_result_feature_names = nullptr) const;

		// Get labels of all observations
		Eigen::VectorXd get_labels() const;

		// Get labels but converted to binary (threshold is used to divide classes into -1 and 1 case, as demanded by shogun. value > threshold is converted to 1, else -1)
		Eigen::VectorXd get_binary_labels(int threshold = 0.0) const;

		// Get feature count
		int get_feature_count() const;

		// Get names of features (though it is a vector, each name is unique)
		std::vector<std::string> get_feature_names() const;

		// Save in CSV files
		void save_as_CSV(std::string features_file_path, std::string labels_file_path) const;
		void save_features_as_CSV(std::string file_path) const;
		void save_labels_as_CSV(std::string file_path) const;

	private:

		// Custom data structure for feature names. Uses ordered vector but guarantess unique names
		class Names
		{
		public:

			Names() {}
			Names(const std::vector<std::string>& r_names, bool force_unique = true)
			{
				if (force_unique)
				{
					for (const auto& r_name : r_names)
					{
						add(r_name);
					}
				}
				else
				{
					_names = r_names;
				}
			}

			void add(const std::string& r_name, bool force_unique = true)
			{
				// Super slow, but very few executions
				if (force_unique)
				{
					for (const auto& r_known_name : _names)
					{
						if (r_name == r_known_name)
						{
							return;
						}
					}
				}
				_names.push_back(r_name);
			}

			const std::vector<std::string>& get() const
			{
				return _names;
			}

			bool is_same(const std::vector<std::string>& names) const
			{
				if (names.size() == _names.size())
				{
					for (int i = 0; i < (int)_names.size(); ++i)
					{
						if (names.at(i) != _names.at(i))
						{
							return false;
						}
					}
				}
				else
				{
					return false;
				}
				return true;
			}

		private:
			std::vector<std::string> _names;
		};

		// Get index-based min-max values
		std::vector<std::pair<double, double> > get_min_max_internal() const;

		// Members
		Names _names; // count is same as columns in _values
		Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> _values; // stored row-wise
		Eigen::VectorXd _labels; // count same as rows in _values
		const double _init_value; // defined by constructor
	};
}
