#include "Model.hpp"
#include <Core/Core.hpp>

namespace stage
{
	namespace merging
	{
		namespace model
		{
			core::long64 compute(
				std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
				std::shared_ptr<const data::State> a,
				std::shared_ptr<const data::State> b)
			{
				// Note: EVERYTHING HERE MUST BE THREAD-SAFE!

				// Overlap and crop the two input images
				auto sp_overlap_a = std::make_shared<cv::Mat>();
				auto sp_overlap_b = std::make_shared<cv::Mat>();
				bool overlap = core::opencv::overlap_and_crop(
					a->get_stitched_screenshot(),
					b->get_stitched_screenshot(),
					*sp_overlap_a,
					*sp_overlap_b);

				if (!overlap)
				{
					return 0; // do not merge
				}
				else
				{
					bool visual_change = false;
					if (!core::opencv::pixel_perfect_same(*sp_overlap_a, *sp_overlap_b)) // only compute features if required
					{
						// Feature vector
						feature::FeatureVector feature_vector(sp_overlap_a, sp_overlap_b);
						auto features = feature_vector.get();

						// Store feature observations
						auto sp_dataset = std::make_shared<data::Dataset>(feature_vector.get_names());
						sp_dataset->append_observation(features);
						sp_dataset->normalize(sp_classifier->get_min_max());

						// Classify the observation using the trained random forest
						auto sp_labels = sp_classifier->classify(sp_dataset);

						// Check for label of classified observation
						visual_change = (*sp_labels)(0) > 0.0;
					}

					// Provide feedback
					if (!visual_change) // should merge when there is no visual change
					{
						core::long64 overlap_pixel_count = 0;

						// Go over pixels of both matrices
						for (unsigned int x = 0; x < (unsigned int)sp_overlap_a->cols; ++x)
						{
							for (unsigned int y = 0; y < (unsigned int)sp_overlap_a->rows; ++y)
							{
								// Retrieve pixel values
								const auto& current_pixel = sp_overlap_a->at<cv::Vec4b>(y, x);
								const auto& potential_pixel = sp_overlap_b->at<cv::Vec4b>(y, x);

								// Check alpha value
								if (current_pixel[3] <= 0 || potential_pixel[3] <= 0) { continue; }

								// Increment count of pixels
								++overlap_pixel_count;
							}
						}
						
						// Further measure are available
						// auto frame_count_a = a->get_total_frame_count();
						// auto frame_count_b = b->get_total_frame_count();
						// auto session_count_a = a->get_total_session_count();
						// auto session_count_b = b->get_total_session_count();

						return overlap_pixel_count; // score of similarity is nothing else than overlapping pixel count
					}
					else
					{
						return 0;
					}
				}
			}
		}
	}
}
