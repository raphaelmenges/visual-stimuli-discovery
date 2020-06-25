#pragma once

#include <Learn/Classifier.hpp>
#include <shogun/machine/RandomForest.h>

namespace learn
{
	class RandomForest : public MulticlassClassifier<shogun::CRandomForest>
	{
	public:

		// Constructor, trains classifier
		RandomForest(std::shared_ptr<const data::Dataset> sp_dataset);

		// Destructor
		virtual ~RandomForest();

		// Print classifier
		virtual void print() const;

		// Get out-of-bag-error
		double get_out_of_bag_error() const
		{
			auto acc = shogun::some<shogun::CMulticlassAccuracy>();
			return (*_sp_classifier)->get_oob_error(acc);
		}
	};
}