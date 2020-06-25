//! Processing stage.
/*!
Processing stage takes sessions as input and outputs corresponding log dates.
*/

#pragma once

#include <Data/Session.hpp>
#include <Data/LogDatum.hpp>
#include <Core/VisualDebug.hpp>

namespace stage
{
	namespace processing
	{
		std::shared_ptr<data::LogDatumContainers_const> run(
			VD(core::visual_debug::Explorer& r_visual_explorer, )
			std::shared_ptr<data::Sessions_const> sp_sessions);
	}
}