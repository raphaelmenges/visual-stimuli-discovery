#include "Dataset.hpp"
#include <Core/Core.hpp>
#include <fstream>

namespace data
{
	Dataset::Dataset(
		const std::vector<std::string>& r_feature_names,
		double init_value)
		:
		_init_value(init_value)
	{
		_names = Names(r_feature_names);
		_values.resize(0, _names.get().size()); // zero rows, feature-many columns
	}

	Dataset::Dataset(
		std::string features_file_path,
		std::string labels_file_path,
		double init_value)
		:
		_init_value(init_value)
	{
		// First, read features
		if(!(features_file_path.empty()))
		{
			std::ifstream features(features_file_path);
			std::string line;

			// Read header, aka names of features
			std::getline(features, line, '\n');
			auto tokens = core::misc::tokenize(line);
			_names = Names(tokens, false); // do not enforce unique names on imported data. just take as is
			int feature_count = (int)_names.get().size();

			// Read features
			while (std::getline(features, line, '\n'))
			{
				if (!line.empty())
				{
					// Resize matrix for appending new observation
					auto row_count = _values.rows();
					_values.conservativeResize(row_count + 1, feature_count);

					// Go over tokens and fill row of feature matrix
					auto tokens = core::misc::tokenize(line);
					for (int i = 0; i < (int)tokens.size(); ++i)
					{
						_values(row_count, i) = std::atof(tokens.at(i).c_str()); // BEWARE! This might break for German setup if comma is expected instead of dot
					}
				}
			}
		}
		else
		{
			// TODO: count labels and create empty observations? or just assertion?
		}

		// Second, store labels
		if (!(labels_file_path.empty()))
		{
			std::ifstream labels(labels_file_path);
			std::string line;
			while (std::getline(labels, line, '\n'))
			{
				if (!line.empty())
				{
					auto labels_count = _labels.size();
					_labels.conservativeResize(labels_count + 1);
					_labels(labels_count) = std::atof(line.c_str());

				}
			}
		}
		else // no labels file provided, init labels with init value
		{
			_labels.resize(_values.rows());
			for (int i = 0; _labels.size(); ++i)
			{
				_labels(i) = init_value;
			}
		}
	}

	Dataset::Dataset(
		std::string features_file_path,
		double init_value)
		:
		Dataset(features_file_path, "", init_value)
	{
		// Other constructor invocated
	}

	void Dataset::append_observation(const std::map<std::string, double>& r_features, double label)
	{
		// Resize matrix for new observation
		auto row_count = _values.rows();
		_values.conservativeResize(row_count + 1, _values.cols());

		// Go over known names and fill corresponding entry in values
		for (int i = 0; i < (int)_names.get().size(); ++i)
		{
			auto entry = r_features.find(_names.get().at(i));
			if (entry != r_features.end())
			{
				_values(row_count, i) = entry->second; // fill field with provided value
			}
			else
			{
				_values(row_count, i) = _init_value; // at least, initialize field
			}
		}

		// Remember label
		auto labels_count = _labels.size();
		_labels.conservativeResize(labels_count + 1);
		_labels(labels_count) = label;
	}
	
	void Dataset::filter_features(const std::vector<std::string>& r_feature_names)
	{
		std::vector<std::string> new_names;
		_values = get_observations_row_wise(r_feature_names, &new_names); // overwrites values
		_names = Names(new_names, false); // overwrites names
	}

	void Dataset::set_labels(Eigen::VectorXd labels)
	{
		_labels = labels;
	}

	void Dataset::normalize()
	{
		auto min_max = get_min_max_internal();

		// Go over values and normalize
		for (int i = 0; i < (int)_values.size(); ++i)
		{
			auto feature_idx = i % (int)_names.get().size();
			auto min = min_max.at(feature_idx).first;
			auto max = min_max.at(feature_idx).second;
			auto diff = max - min;
			if (diff > 0)
			{
				*(_values.data() + i) = ((*(_values.data() + i)) - min) / diff;
			}
			else
			{
				*(_values.data() + i) = 0.0;
			}
		}
	}

	void Dataset::normalize(const std::map<std::string, std::pair<double, double> >& r_min_max)
	{
		// Go over entries in map
		for (const auto& r_entry : r_min_max)
		{
			// Check for index of entry in own values structure
			int pos = (int)std::distance(_names.get().begin(), std::find(_names.get().begin(), _names.get().end(), r_entry.first));

			// Perform normalization
			if (pos < (int)_names.get().size())
			{
				auto min = r_entry.second.first;
				auto max = r_entry.second.second;
				auto diff = max - min;

				// Go over entries in the specific column
				for (int i = 0; i < _values.rows(); ++i)
				{
					if (diff > 0)
					{
						_values(i, pos) = (_values(i, pos) - min) / diff;
					}
					else
					{
						_values(i, pos) = 0.0;
					}
				}
			}
		}
	}

	std::map<std::string, std::pair<double, double> > Dataset::get_min_max() const
	{
		std::map<std::string, std::pair<double, double> > min_max;
		auto temp = get_min_max_internal();
		for (int i = 0; i < (int)_names.get().size(); ++i)
		{
			min_max[_names.get().at(i)] = temp.at(i);
		}
		return min_max;
	}

	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
		Dataset::get_observations_row_wise(const std::vector<std::string>& r_feature_names, std::vector<std::string>* p_result_feature_names) const
	{
		// Check, whether names fit exactly the ones stored
		if (_names.is_same(r_feature_names))
		{
			if(p_result_feature_names) { *p_result_feature_names = _names.get(); }
			return _values;
		}
		else // if not, create new matrix with feature values as requested
		{
			// Get indices of features in stored dataset
			if(p_result_feature_names) { p_result_feature_names->clear(); }
			std::vector<int> indices;
			for (int i = 0; i < (int)r_feature_names.size(); ++i) // go over input names
			{
				for (int j = 0; j < (int)_names.get().size(); ++j) // go over stored names
				{
					if (r_feature_names.at(i) == _names.get().at(j)) // match names
					{
						indices.push_back(j); // store index of the found feature
						if(p_result_feature_names) { p_result_feature_names->push_back(_names.get().at(j)); } // store feature name
						break;
					}
				}
			}

			// Create a new matrix and fill it to fit the requirements provided
			int obs_count = (int)_labels.size();
			int feature_count = (int)indices.size(); // count of found features
			Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> matrix;
			matrix.resize(obs_count, feature_count);
			for (int j = 0; j < feature_count; ++j) // column
			{
				for (int i = 0; i < obs_count; ++i) // row, store observations of that feature
				{
					matrix(i, j) = _values(i, indices.at(j));;
				}
			}

			return matrix;
		}
	}
	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>
		Dataset::get_observations_column_wise(const std::vector<std::string>& r_feature_names, std::vector<std::string>* p_result_feature_names) const
	{
			return Dataset::get_observations_row_wise(r_feature_names, p_result_feature_names).transpose();
	}

	Eigen::VectorXd Dataset::get_labels() const
	{
		return _labels;
	}

	Eigen::VectorXd Dataset::get_binary_labels(int threshold) const
	{
		Eigen::VectorXd binary_labels(_labels.size());
		for (int i = 0; i < (int)_labels.size(); ++i)
		{
			binary_labels(i) = _labels(i) > threshold ? 1.0 : -1.0;
		}
		return binary_labels;
	}

	int Dataset::get_feature_count() const
	{
		return (int)_names.get().size();
	}

	std::vector<std::string> Dataset::get_feature_names() const
	{
		return _names.get();
	}

	void Dataset::save_as_CSV(std::string features_file_path, std::string labels_file_path) const
	{
		save_features_as_CSV(features_file_path);
		save_labels_as_CSV(labels_file_path);
	}

	void Dataset::save_features_as_CSV(std::string file_path) const
	{
		std::ofstream features(file_path);

		// First, add header
		for (int i = 0; i < (int)_names.get().size(); ++i)
		{
			features << _names.get().at(i);
			if(i + 1 != (int)_names.get().size())
			{
				features << ",";
			}
		}
		features << "\n";

		// Then, go over row-major values matrix and store values
		for (int i = 0; i < _values.size(); ++i)
		{
			// Make string out of value
			std::string value_string = "-1";
			try
			{
				// to_string might throw exception
				double value = *(_values.data() + i);
				if(!std::isinf(value))
				{
					value_string = std::to_string(value);
				}
			}
			catch (...)
			{
				// do nothing, leave value_string initialized
			}
		
			// Store value
			features << value_string;

			// Decided between end of line or separator
			if ((i + 1) % (int)_names.get().size() == 0)
			{
				features << "\n";
			}
			else
			{
				features << ",";
			}
		}
	}

	void Dataset::save_labels_as_CSV(std::string file_path) const
	{
		std::ofstream labels(file_path);
		for (int i = 0; i < (int)_labels.size(); ++i)
		{
			// Store value
			double value = *(_labels.data() + i);
			labels << std::to_string(value) << "\n";
		}
	}

	std::vector<std::pair<double, double> > Dataset::get_min_max_internal() const
	{
		// Get min and max values of each observed feature
		std::vector<std::pair<double, double> > min_max(
			_names.get().size(), // number of features
			std::make_pair(std::numeric_limits<double>::max(), std::numeric_limits<double>::min())); // min and max value

		// Go over all values
		for (int i = 0; i < _values.size(); ++i)
		{
			auto feature_idx = i % (int)_names.get().size();
			double value = *(_values.data() + i);
			double& r_min = min_max.at(feature_idx).first;
			double& r_max = min_max.at(feature_idx).second;
			r_min = value < r_min ? value : r_min;
			r_max = value > r_max ? value : r_max;
		}

		return min_max;
	}
}
