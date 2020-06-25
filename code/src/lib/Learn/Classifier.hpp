//! Abstract base class for classifier.
/*!
This abstract base class for all kind of classifiers. Handles storage of data.
*/

#pragma once

#include <Learn/Learn.hpp>
#include <Data/Dataset.hpp>
#include <shogun/base/some.h>
#include <shogun/lib/SGMatrix.h>
#include <shogun/lib/SGVector.h>
#include <shogun/features/DenseFeatures.h>
#include <shogun/labels/BinaryLabels.h>
#include <shogun/labels/MulticlassLabels.h>
#include <shogun/evaluation/ContingencyTableEvaluation.h>
#include <shogun/evaluation/MulticlassAccuracy.h>
#include <memory>
#include <vector>

namespace learn
{
	// General classifier
	template<typename T>
	class Classifier :  public Learn
	{
	public:

		// Constructor, trains classifier
		Classifier(std::shared_ptr<const data::Dataset> sp_dataset)
		{
			// Feature names
			_feature_names = sp_dataset->get_feature_names();

			// Features
			const auto dataset_features = sp_dataset->get_observations_column_wise(_feature_names); // as required by shogun
			_sp_feature_matrix = std::make_shared<shogun::SGMatrix<double> >((int)dataset_features.rows(), (int)dataset_features.cols());
			for (int i = 0; i < (int)dataset_features.size(); ++i)
			{
				(*_sp_feature_matrix).matrix[i] = *(dataset_features.data() + i);
			}
			_sp_features = std::make_shared<shogun::Some<shogun::CDenseFeatures<double> > >(shogun::some<shogun::CDenseFeatures<double> >(*_sp_feature_matrix));

			// Features are continuous, marked with a bool set to false
			int feature_count = (int)dataset_features.rows(); // each row are records of a single feature, each column an observation
			_sp_feature_types = std::make_shared<shogun::SGVector<bool> >(feature_count);
			for (int i = 0; i < feature_count; ++i)
			{
				(*_sp_feature_types)[i] = false;
			}

			// Labels are loaded by subclasses
		}

		// Classification
		virtual std::shared_ptr<const Eigen::VectorXd> classify(std::shared_ptr<const data::Dataset> sp_dataset) const = 0;

		// Print classifier
		virtual void print() const = 0;

	protected:

		// Training
		std::vector<std::string> _feature_names;
		std::shared_ptr<shogun::SGMatrix<double> >_sp_feature_matrix;
		std::shared_ptr<shogun::Some<shogun::CDenseFeatures<double> > > _sp_features;
		std::shared_ptr<shogun::SGVector<bool> > _sp_feature_types;

		// Classifier
		std::shared_ptr<shogun::Some<T> > _sp_classifier;
	};

	// Binary classifier
	template<typename T>
	class BinaryClassifier : public Classifier<T>
	{
	public:

		// Constructor, trains classifier
		BinaryClassifier(std::shared_ptr<const data::Dataset> sp_dataset)
			:
			Classifier<T>(sp_dataset)
		{
			// Labels
			const auto dataset_labels = sp_dataset->get_binary_labels();
			_sp_label_vector = std::make_shared<shogun::SGVector<double> >((int)dataset_labels.size());
			for (int i = 0; i < (int)dataset_labels.size(); ++i)
			{
				(*_sp_label_vector)[i] = *(dataset_labels.data() + i);
			}
			_sp_labels = std::make_shared<shogun::Some<shogun::CBinaryLabels> >(shogun::some<shogun::CBinaryLabels>(*_sp_label_vector));
		}

		// Get training accuracy
		double training_accuracy() const
		{
			auto labels_predict = (*(this->_sp_classifier))->apply_binary(*(this->_sp_features));
			auto eval = shogun::some<shogun::CAccuracyMeasure>();
			return eval->evaluate(labels_predict, *_sp_labels);
		}

		// Classification
		virtual std::shared_ptr<const Eigen::VectorXd> classify(std::shared_ptr<const data::Dataset> sp_dataset) const
		{
			// TODO: match features names of trained dataset and incoming one? assertion at mismatch?

			const auto dataset_features = sp_dataset->get_observations_column_wise(this->_feature_names);
			auto feature_matrix = shogun::SGMatrix<double>((int)dataset_features.rows(), (int)dataset_features.cols());
			for (int i = 0; i < (int)dataset_features.size(); ++i)
			{
				feature_matrix.matrix[i] = *(dataset_features.data() + i);
			}
			auto features = shogun::some<shogun::CDenseFeatures<double> >(feature_matrix);
			auto labels_predict = (*(this->_sp_classifier))->apply_binary(features);
			auto sp_vector = std::make_shared<Eigen::VectorXd>(labels_predict->get_num_labels());
			auto vector = labels_predict->get_labels();
			for (int i = 0; i < vector.size(); ++i)
			{
				(*sp_vector)(i) = vector[i] > 0.0 ? 1.0 : 0.0; // convert binary encoding {-1,1} to multiclass encoding
			}
			return sp_vector;
		}

	protected:

		// Training
		std::shared_ptr<shogun::SGVector<double> >_sp_label_vector;
		std::shared_ptr<shogun::Some<shogun::CBinaryLabels> > _sp_labels;
	};

	// Multiclass classifier
	template<typename T>
	class MulticlassClassifier : public Classifier<T>
	{
	public:

		// Constructor, trains classifier
		MulticlassClassifier(std::shared_ptr<const data::Dataset> sp_dataset)
			:
			Classifier<T>(sp_dataset)
		{
			// Labels
			const auto dataset_labels = sp_dataset->get_labels();
			_sp_label_vector = std::make_shared<shogun::SGVector<double> >((int)dataset_labels.size());
			for (int i = 0; i < (int)dataset_labels.size(); ++i)
			{
				(*_sp_label_vector)[i] = *(dataset_labels.data() + i);
			}
			_sp_labels = std::make_shared<shogun::Some<shogun::CMulticlassLabels> >(shogun::some<shogun::CMulticlassLabels>(*_sp_label_vector));
		}

		// Get training accuracy
		double training_accuracy() const
		{
			auto labels_predict = (*(this->_sp_classifier))->apply_multiclass(*(this->_sp_features));
			auto eval = shogun::some<shogun::CMulticlassAccuracy>();
			return eval->evaluate(labels_predict, *_sp_labels);
		}

		// Classification
		virtual std::shared_ptr<const Eigen::VectorXd> classify(std::shared_ptr<const data::Dataset> sp_dataset) const
		{
			// TODO: match features names of trained dataset and incoming one? assertion at mismatch?

			const auto dataset_features = sp_dataset->get_observations_column_wise(this->_feature_names);
			auto feature_matrix = shogun::SGMatrix<double>((int)dataset_features.rows(), (int)dataset_features.cols());
			for (int i = 0; i < (int)dataset_features.size(); ++i)
			{
				feature_matrix.matrix[i] = *(dataset_features.data() + i);
			}
			auto features = shogun::some<shogun::CDenseFeatures<double> >(feature_matrix);
			auto labels_predict = (*(this->_sp_classifier))->apply_multiclass(features);
			auto sp_vector = std::make_shared<Eigen::VectorXd>(labels_predict->get_num_labels());
			auto vector = labels_predict->get_labels();
			for (int i = 0; i < vector.size(); ++i)
			{
				(*sp_vector)(i) = vector[i];
			}
			return sp_vector;
		}

	protected:

		// Training
		std::shared_ptr<shogun::SGVector<double> >_sp_label_vector;
		std::shared_ptr<shogun::Some<shogun::CMulticlassLabels> > _sp_labels;
	};
}
