//! Splitting stage.
/*!
Splitting stage takes log dates and outputs intra-user states.
*/

#pragma once

#include <Core/VisualChangeClassifier.hpp>
#include <Data/LogDatum.hpp>
#include <Data/IntraUserState.hpp>
#include <Core/VisualDebug.hpp>

namespace stage
{
	namespace splitting
	{
		std::shared_ptr<data::IntraUserStateContainers_const> run(
			VD(core::visual_debug::Explorer& r_visual_explorer, )
			std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
			std::shared_ptr<data::LogDatumContainers_const> sp_log_datum_containers);
	}
}