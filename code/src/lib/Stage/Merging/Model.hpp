//! Merge model interface and implementations.
/*!
Provides decision whether merge of states (intra or inter) into inter-user state should be performed.
In contrast to the model of splitting stage, this does not take a visual debug object. Instead,
this model is thread-safe and can be computed in parallel multiple times.
*/

#pragma once

#include <Core/VisualChangeClassifier.hpp>
#include <Data/State.hpp>
#include <memory>

namespace stage
{
	namespace merging
	{
		namespace model
		{
			// Returns score higher zero when merge is highly recommended and zero if not
			core::long64 compute(
				std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
				std::shared_ptr<const data::State> a,
				std::shared_ptr<const data::State> b);
		}
	}
}
