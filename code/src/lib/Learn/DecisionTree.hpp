#pragma once

#include <Learn/Classifier.hpp>
#include <shogun/multiclass/tree/CARTree.h>

namespace learn
{
	class DecisionTree : public MulticlassClassifier<shogun::CCARTree>
	{
	public:

		// Constructor, trains classifier
		DecisionTree(std::shared_ptr<const data::Dataset> sp_dataset);

		// Destructor
		virtual ~DecisionTree();

		// Print classifier
		virtual void print() const;
	};
}