#include "Splitter.hpp"
#include <Stage/Splitting/Model.hpp>
#include <Core/Core.hpp>
#include <Util/LayerComparator.hpp>

const int WITHDRAW_THRESHOLD = core::mt::get_config_value(32, { "splitting", "splitter", "withdraw_treshold" });

namespace stage
{
	namespace splitting
	{
		Splitter::Splitter(
			VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
			std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
			std::shared_ptr<const data::LogDatumContainer> sp_log_datum_container)
			:
			Work(VD(sp_dump, ) core::PrintReport(sp_log_datum_container->get_session()->get_id())), // initial empty report
			_up_walker(std::unique_ptr<util::LogDatesWalker>(
				new util::LogDatesWalker(sp_log_datum_container->get(), sp_log_datum_container->get_session()->get_webm_path()))),
			_sp_classifier(sp_classifier),
			_sp_log_dates(sp_log_datum_container->get())
		{
			// Initialize product
			_sp_container = std::make_shared<ProductType>(sp_log_datum_container);
		}

		std::shared_ptr<Splitter::ProductType> Splitter::internal_step()
		{
			/////////////////////////////////////////////////
			/// Prepare log datum and viewport pixels
			/////////////////////////////////////////////////

			// Walk one frame in screencast and log dates
			if (_up_walker->step()) // another frame is available
			{
				// Retrieve values for that frame
				auto sp_log_image = _up_walker->get_log_image();
				int frame_idx = _up_walker->get_frame_idx();
				auto layers_to_process = _up_walker->get_layer_packs();

				// Go over layers and erase those which are not of interest for now (TODO: make this more general!)
				std::vector<int> to_be_deleted_layers;
				int i = 0;
				for (const auto& r_pack : layers_to_process)
				{
					// TODO: use this to restrict the processing to only certain layers
					/*
					if (r_pack.sptr->get_type() != data::LayerType::Root)
					{
						to_be_deleted_layers.push_back(i);
					}
					++i;
					*/
				}
				for (int i = (int)to_be_deleted_layers.size() - 1; i >= 0; --i)
				{
					layers_to_process.erase(layers_to_process.begin() + to_be_deleted_layers.at(i));
				}

				// Go over current intra-user states and try to match layers of current frame
				std::vector<unsigned int> to_be_closed_states; // collect current intra-user states that can be closed
				for (
					unsigned int state_idx = 0;
					state_idx < (unsigned int)_current.size();
					++state_idx)
				{
					// Get one current intra-user state
					auto& r_state = _current.at(state_idx);
					bool close_state = true;

					// Get corresponding visual debug datum to fill up
					VD(
					std::shared_ptr<core::visual_debug::Datum> sp_datum = nullptr; // used in check for split
					if (_sp_dump) // check whether there is a dump at all
					{
						sp_datum = _current_vd_split_checks.at(state_idx);
					})

					/////////////////////////////////////////////////
					/// Assign a layer of the current frame to a current intra-user state
					/////////////////////////////////////////////////

					// Retrieve latest layer in that intra-user state (the state still lives in the previous frame)
					auto layer_access = r_state->get_layer_access(frame_idx - 1); // one frame before now
					auto sp_latest_layer = _sp_log_dates->at(frame_idx - 1)->access_layer(layer_access); // get pointer to latest layer of that intra-user state

					// Compare available layers of this frame with that layer from the intra-user state
					int chosen_layer = -1;
					int idx = 0;
					for (const auto& r_pack : layers_to_process) // go over layers of current frame
					{
						if (util::layer_comparator::compare(r_pack.sptr, sp_latest_layer).value() > core::mt::get_config_value(0.5f, { "model", "splitting", "layer_threshold" })) // greedy, just take the first one above a certain threshold
						{
							chosen_layer = idx;
							break;
						}
						++idx;
					}

					/////////////////////////////////////////////////
					/// Chech whether split is required
					/////////////////////////////////////////////////

					// There is a layer successor available in the log datum. Check whether split is now required or not
					if (chosen_layer >= 0)
					{
						// Prepare values
						const auto& r_pack = layers_to_process.at(chosen_layer);
						int scroll_x = (int)r_pack.sptr->get_scroll_x();
						int scroll_y = (int)r_pack.sptr->get_scroll_y();

						// Potential pixels from chosen layer
						auto potential_pixels = sp_log_image->get_layer_pixels(r_pack.sptr->get_view_mask());

						// Create rect that represents potential pixels in current pixels space
						cv::Rect rect(scroll_x, scroll_y, potential_pixels.cols, potential_pixels.rows);

						// Extend cloned stitched screenshot from intra-user state with some emptyness if required
						cv::Mat stitched_screenshot_clone = r_state->get_stitched_screenshot().clone();
						core::opencv::extend(stitched_screenshot_clone, rect);

						// Get portion of stitched screenshot corresponding to potential new pixels
						cv::Mat transformed_current_pixels =
							stitched_screenshot_clone(rect);

						// Compare current pixels and data of the intra-user state and the potential data
						model::Result split_result = model::compute(
							VD(sp_datum, ) // visual debug datum to add on
							_sp_classifier, // classifier of visual change
							transformed_current_pixels, // current pixels, already transformed accordingly to potential pixels
							sp_latest_layer, // current layer
							potential_pixels, // potential pixels to compare against
							r_pack.sptr // potential layer to compare against
						);

						// If no split required, add frame to intra-use state
						if(split_result == model::Result::same)
						{
							// Push this frame onto the intra-user state
							r_state->add_frame(
								r_pack.access, // access to layer
								potential_pixels, // new pixels
								scroll_x, // x-offset of pixels
								scroll_y); // y-offset of pixels

							// Remove the layer from the ones to be processed
							layers_to_process.erase(layers_to_process.begin() + chosen_layer);

							// Remember not to close state as it is still alive and extended
							close_state = false;
						}
						else if(split_result == model::Result::no_overlap) // special case: there was no overlap
						{
							// TODO: use this information somehow? right now, "no_overlap" and "different" are treated the same
						}
					} // end of state extension

					// Decide whether to close state or not
					if (close_state)
					{
						// The layer is not considered as successor and intra-user state is closed
						to_be_closed_states.push_back(state_idx);
					}

				} // end of iteration over current states

				// Go over closed intra-user states and put them into the product of work
				for (int i = (int)to_be_closed_states.size() - 1; i >= 0; --i) // iterate reverse so elements can be removed in vector from back to front with stable indexing
				{
					auto closed_idx = to_be_closed_states.at(i);
					put_to_product(closed_idx);
				}

				/////////////////////////////////////////////////
				/// Handle orphan layers
				/////////////////////////////////////////////////

				// Go over layers not yet mapped layers and initiate a new intra-user state each
				for (const auto& r_pack : layers_to_process)
				{
					// Prepare values
					int scroll_x = (int)r_pack.sptr->get_scroll_x();
					int scroll_y = (int)r_pack.sptr->get_scroll_y();

					// Pixels from chosen layer
					auto layer_pixels = sp_log_image->get_layer_pixels(r_pack.sptr->get_view_mask());

					// Create new intra-user state
					auto up_state = std::unique_ptr<data::IntraUserState>(
						new data::IntraUserState(
							_sp_container,
							frame_idx, // current frame is the first frame of the new intra-user state
							r_pack.access, // access to corresponding layer
							layer_pixels, // initial pixels
							scroll_x, // x-offset of pixels
							scroll_y)); // y-offset of pixels
					_current.push_back(std::move(up_state));

					// Add corresponding visual debug datum to the vector for visual debug datums per intra-user state
					VD(if (_sp_dump) {
						auto sp_datum = vd_datum("Intra-User State");
						_current_vd_split_checks.push_back(sp_datum); // is filled later with the stitched screenshot etc.
					})
				}

				// Not yet done
				return nullptr;
			}
			else // no further frame is available
			{
				// Complete all remaining visual debug datum intra-user states
				for (int i = (int)_current.size() - 1; i >= 0; --i) // iterate reverse so elements can be removed in vector from back to front with stable indexing
				{
					put_to_product(i);
				}

				// Move product out of work
				return _sp_container;
			}
		}

		void Splitter::internal_report(Splitter::ReportType& r_report)
		{
			if (_up_walker->get_frame_count() > 0)
			{
				r_report.set_progress((float)_up_walker->get_frame_idx() / ((float)_up_walker->get_frame_count() - 1.f));
			}
			else
			{
				r_report.set_progress(1.f);
			}
		}
		
		void Splitter::put_to_product(int current_idx)
		{
			// Check, whether intra-user state actually contains any pixels
			auto covered_stitched_screenshot = _current.at(current_idx)->get_covered_stitched_screenshot();
			if (covered_stitched_screenshot.empty()) // empty
			{
				// Take xpath of first layer as layer identifier
				auto idx_start = _current.at(current_idx)->get_frame_idx_start();
				auto idx_end = _current.at(current_idx)->get_frame_idx_end();
				auto layer_access = _current.at(current_idx)->get_layer_access(idx_start);
				std::string xpath = _sp_log_dates->at(idx_start)->access_layer(layer_access)->get_xpath();
				for (unsigned int i = idx_start; i <= idx_end; ++i)
				{
					_sp_container->add_empty_frame(xpath, i);
				}
			}
			else if (covered_stitched_screenshot.rows <= WITHDRAW_THRESHOLD || covered_stitched_screenshot.cols <= WITHDRAW_THRESHOLD) // too small
			{
				// TODO: Tell user about it?
			}
			else // push state into product
			{
				// There is sometimes a single opaque pixel in the left upper corner. It is removed here (or TODO: find out the actual issue causing the pixel to exist)
				auto mat = _current.at(current_idx)->get_stitched_screenshot().clone();
				if(mat.rows > 0 && mat.cols > 1)
				{
					if(mat.at<cv::Vec4b>(0, 1)[3] == 0)
					{
						mat.at<cv::Vec4b>(0, 0)[3] = 0;
						_current.at(current_idx)->set_stitched_screenshot(mat);
					}
				}
			
				// Push state to product
				VD(if (_sp_dump) {
					_current_vd_split_checks.at(current_idx)
						->add(vd_matrices("Stitched Screenshot")
							->add(_current.at(current_idx)->get_stitched_screenshot()))
						->add(vd_strings("Frame Idx Start: ")->add(std::to_string(_current.at(current_idx)->get_frame_idx_start())))
						->add(vd_strings("Frame Idx End: ")->add(std::to_string(_current.at(current_idx)->get_frame_idx_end())));
					_sp_dump->add(_current_vd_split_checks.at(current_idx)); // add intra-user state debug datum to the dump
				})
				_sp_container->push_back(std::move(_current.at(current_idx))); // move from local vector to product (make shared out of unique)
			}

			// Remove state from current splitting process
			VD(if (_sp_dump) {
				_current_vd_split_checks.erase(_current_vd_split_checks.begin() + current_idx);
			})
			_current.erase(_current.begin() + current_idx); // remove from vector with current intra-user states
		}
	}
}
