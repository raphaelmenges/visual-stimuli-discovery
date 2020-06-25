#include "SVM.hpp"

#include <Core/Core.hpp>
#include <shogun/kernel/GaussianKernel.h>

namespace learn
{
	SVM::SVM(std::shared_ptr<const data::Dataset> sp_dataset)
		:
		BinaryClassifier<shogun::CLibSVM>(sp_dataset)
	{
		// TODO: crashes, if all labels are the same

		// Create classifier
		auto C = 1.0;
		auto epsilon = 0.001;
		auto gauss_kernel = shogun::some<shogun::CGaussianKernel>(*_sp_features, *_sp_features, 15);
		_sp_classifier = std::make_shared<shogun::Some<shogun::CLibSVM> >(shogun::some<shogun::CLibSVM>(C, gauss_kernel, *_sp_labels));
		(*_sp_classifier)->set_epsilon(epsilon);
		(*_sp_classifier)->train();
	}

	// Destructor
	SVM::~SVM()
	{
		// Nothing to do
	}
	void SVM::print() const
	{
		core::mt::log_info("Printing SVM classifier...not yet implemented");
	}
}