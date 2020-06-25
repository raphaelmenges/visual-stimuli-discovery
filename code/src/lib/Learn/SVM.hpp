#pragma once

#include <Learn/Classifier.hpp>
#include <shogun/classifier/svm/LibSVM.h>

namespace learn
{
	class SVM : public BinaryClassifier<shogun::CLibSVM>
	{
	public:

		// Constructor, trains classifier
		SVM(std::shared_ptr<const data::Dataset> sp_dataset);

		// Destructor
		virtual ~SVM();

		// Print classifier
		virtual void print() const;
	};
}