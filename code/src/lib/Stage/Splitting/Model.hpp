//! Split model interface and implementations.
/*!
Provides decision whether split of log dates into different intra-user states should be performed.
*/

#pragma once

#include <Core/VisualChangeClassifier.hpp>
#include <Data/Layer.hpp>
#include <Core/VisualDebug.hpp>
#include <opencv2/core/types.hpp>
#include <memory>

namespace stage
{
	namespace splitting
	{
		namespace model
		{
			enum class Result {
				same,
				different,
				no_overlap
			};

			// Check whether split it recommended
			Result compute(
				VD(std::shared_ptr<core::visual_debug::Datum> sp_datum, )
				std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
				const cv::Mat transformed_current_pixels,
				std::shared_ptr<const data::Layer> sp_current_layer,
				const cv::Mat potential_pixels,
				std::shared_ptr<const data::Layer> sp_potential_layer);
		}
	}
}
