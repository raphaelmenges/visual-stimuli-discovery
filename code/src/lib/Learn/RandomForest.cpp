#include "RandomForest.hpp"

#include <Core/Core.hpp>
#include <shogun/ensemble/MajorityVote.h>
#include <deque>

namespace learn
{
	RandomForest::RandomForest(std::shared_ptr<const data::Dataset> sp_dataset)
		:
		MulticlassClassifier<shogun::CRandomForest>(sp_dataset)
	{
		// Create classifier
		auto vote = shogun::some<shogun::CMajorityVote>();
		_sp_classifier = std::make_shared<shogun::Some<shogun::CRandomForest> >(shogun::some<shogun::CRandomForest>(*_sp_features, *_sp_labels, 100));
		(*_sp_classifier)->set_combination_rule(vote);
		(*_sp_classifier)->set_feature_types(*_sp_feature_types);
		(*_sp_classifier)->train();
	}

	// Destructor
	RandomForest::~RandomForest()
	{
		// Nothing to do
	}

	void RandomForest::print() const
	{
		core::mt::log_info("Printing random forest classifier...not yet implemented");
	}
}