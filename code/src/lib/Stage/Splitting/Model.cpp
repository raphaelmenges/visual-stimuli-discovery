#include "Model.hpp"
#include <Core/Core.hpp>
#include <opencv2/opencv.hpp>

namespace stage
{
	namespace splitting
	{
		namespace model
		{
			Result compute(
				VD(std::shared_ptr<core::visual_debug::Datum> sp_datum, )
				std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
				const cv::Mat transformed_current_pixels,
				std::shared_ptr<const data::Layer> sp_current_layer,
				const cv::Mat potential_pixels,
				std::shared_ptr<const data::Layer> sp_potential_layer)
			{
				assert(("model::split::Simple: current pixels and potential pixels have different format",
					transformed_current_pixels.rows == potential_pixels.rows
					&& transformed_current_pixels.cols == potential_pixels.cols
					&& transformed_current_pixels.channels() == potential_pixels.channels()));

				// Overlap and crop the two input images
				auto sp_overlap_current = std::make_shared<cv::Mat>();
				auto sp_overlap_potential = std::make_shared<cv::Mat>();
				bool overlap = core::opencv::overlap_and_crop(
					transformed_current_pixels,
					potential_pixels,
					*sp_overlap_current,
					*sp_overlap_potential);

				if (!overlap)
				{
					return Result::no_overlap;
				}
				else
				{
					// Prepare visual debug output
					VD(
					std::shared_ptr<core::visual_debug::Datum> sp_split_datum = nullptr;
					std::shared_ptr<core::visual_debug::Datum> sp_current_details_datum = nullptr;
					std::shared_ptr<core::visual_debug::Datum> sp_potential_details_datum = nullptr;
					if (sp_datum)
					{
						sp_split_datum = vd_datum("Simple Split Model");
						sp_datum->add(sp_split_datum);
						sp_split_datum->add(vd_matrices("Current and potential pixels")
							->add(transformed_current_pixels)
							->add(potential_pixels));
						sp_split_datum->add(vd_matrices("Current and potential pixels, overlapping")
							->add(*sp_overlap_current)
							->add(*sp_overlap_potential));
					})

					// Check for perfect similarity
					if (core::opencv::pixel_perfect_same(*sp_overlap_current, *sp_overlap_potential))
					{
						return Result::same; // do not split
					}

					// Feature vector
					feature::FeatureVector feature_vector(sp_overlap_current, sp_overlap_potential);
					auto features = feature_vector.get();

					// Store feature observations
					auto sp_dataset = std::make_shared<data::Dataset>(feature_vector.get_names());
					sp_dataset->append_observation(features);
					sp_dataset->normalize(sp_classifier->get_min_max());

					// Classify the observation using the trained random forest
					auto sp_labels = sp_classifier->classify(sp_dataset);
	
					// Check for label of classified observation
					if ((*sp_labels)(0) > 0.0)
					{
						return Result::different;
					}
					else
					{
						return Result::same;
					}
				}
			}
		}
	}
}
