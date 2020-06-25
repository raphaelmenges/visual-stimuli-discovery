//! Merging stage.
/*!
Merging stage takes intra-user states and outputs inter-user states.
*/

// TODO: One problem if only one session is processed: split and merge are quite similar,
// thus what has been split would be probably not merged

#pragma once

#include <Core/VisualChangeClassifier.hpp>
#include <Data/IntraUserState.hpp>
#include <Data/InterUserState.hpp>
#include <Core/VisualDebug.hpp>

namespace stage
{
	namespace merging
	{
		std::shared_ptr<data::InterUserStateContainers_const> run(
			VD(core::visual_debug::Explorer& r_visual_explorer, )
			std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
			std::shared_ptr<data::IntraUserStateContainers_const> sp_intra_user_state_containers);
	}
}