#include "Cleaner.hpp"
#include <Core/Core.hpp>
#include <Util/Clusterer.hpp>

const int FRAME_COUNT = core::mt::get_config_value(1, { "splitting", "cleaner", "frame_count" });
const int ITERATION_COUNT = core::mt::get_config_value(3, { "splitting", "cleaner", "iteration_count" });

namespace stage
{
	namespace splitting
	{
		Cleaner::Cleaner(
			VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
			std::shared_ptr<data::IntraUserStateContainer> sp_intra_user_state_container)
			:
			Work(VD(sp_dump, ) core::PrintReport(sp_intra_user_state_container->get_session()->get_id())) // initial empty report
		{
			// Initialize product (container is modified, not replaced)
			_sp_container = sp_intra_user_state_container;
		}

		std::shared_ptr<Cleaner::ProductType> Cleaner::internal_step()
		{
			// Below assumes that the intra-user states are from one a single session
			// (otherwise frame indices might be out of range here)

			// Prepare visual debug
			VD(
			std::shared_ptr<core::visual_debug::Datum> sp_merged_stable_datum = nullptr;
			std::shared_ptr<core::visual_debug::Datum> sp_orphan_candidates_datum = nullptr;
			if (_sp_dump)
			{
				// General dates
				sp_merged_stable_datum = vd_datum("Merged Stable States");
				_sp_dump->add(sp_merged_stable_datum);
				sp_orphan_candidates_datum = vd_datum("Orphan Candidate States");
				_sp_dump->add(sp_orphan_candidates_datum);
			})

			// Compute clusters of intra-user states belonging to the same layer
			std::shared_ptr<data::IntraUserStates> sp_intras = _sp_container->get();
			auto clusters = util::clusterer::compute<data::IntraUserState>(*sp_intras);

			// Intra-user states are now in the clusters object, clear the container from them
			_sp_container->clear(); // cannot just create a new container because there are more members in the container that would have to be copied

			// Perform cleaning for each cluster separately
			for(auto& rsp_cluster : clusters)
			{
				// Prepare visual debug
				VD(
				std::shared_ptr<std::map<int, std::shared_ptr<core::visual_debug::Datum> > > sp_stable_dates = nullptr; // idx to visual debug datum
				if (_sp_dump)
				{
					sp_stable_dates = std::make_shared<std::map<int, std::shared_ptr<core::visual_debug::Datum> > >();
				})
			
				// Stable states stay stable over iterations. Candidates are merged into stable
				std::set<int> stable_idxs; // those states into which can be merged
				std::set<int> candidate_idxs; // those states which should be merged into stable states

				// Partion intra-user states into candidates and stable states
				for(int i = 0; i < (int)rsp_cluster->size(); ++i)
				{
					auto frame_count = rsp_cluster->at(i)->get_frame_count();
					if((int)frame_count <= FRAME_COUNT)
					{
						candidate_idxs.emplace(i);
					}
					else
					{
						stable_idxs.emplace(i);
						VD(if(sp_stable_dates) // create initial map entries, one for each stable state
						{
							const auto sp_stable = rsp_cluster->at(i);
							auto sp_stable_datum = vd_datum("Stable");
							sp_stable_datum->add(vd_matrices("Stitched Screenshot")
							->add(sp_stable->get_stitched_screenshot()));
							sp_stable_datum->add(vd_strings("initial_frame_idx_start")
							->add(std::to_string(sp_stable->get_frame_idx_start())));
							sp_stable_datum->add(vd_strings("initial_frame_idx_end")
							->add(std::to_string(sp_stable->get_frame_idx_end())));
							(*sp_stable_dates)[i] = sp_stable_datum;
						})
					}
				}
				
				// Iterate and merge candidates into stable states
				std::vector<int> intras_to_be_removed;
				for(int iteration = 0; iteration < ITERATION_COUNT; ++iteration)
				{
					// Break iteration if there are not more candidates
					if(candidate_idxs.empty())
					{
						break;
					}

					// For each iteration, merge into stable state only once from each side
					std::map<int, std::pair<bool, bool> > stable_touched; // stable idx to touched
					for(int stable_idx : stable_idxs)
					{
						stable_touched[stable_idx] = {false, false}; // false means "not yet touched"
					}
	
					// Go over candidates to merge and decide into which neighboring stable state to merge
					std::vector<int> candidates_to_be_removed;
					for(int candidate_idx : candidate_idxs)
					{
						auto sp_candidate = rsp_cluster->at(candidate_idx);
						int candidate_prev_frame = ((int)sp_candidate->get_frame_idx_start()) - 1;
						int candidate_next_frame = ((int)sp_candidate->get_frame_idx_end()) + 1;
	
						// Check stable intra-user states for start and end frames
						for(int stable_idx : stable_idxs)
						{
							bool merged = false;
							bool& r_touched_left = stable_touched[stable_idx].first;
							bool& r_touched_right = stable_touched[stable_idx].second;
							auto sp_stable =  rsp_cluster->at(stable_idx);
							int stable_start = sp_stable->get_frame_idx_start();
							int stable_end = sp_stable->get_frame_idx_end();

							if(!r_touched_left && candidate_next_frame == stable_start) // candidate is directly before stable and stable not yet touched on that side in this iteration
							{
								// Push frames of candidate to stable (backwards)
								for(
									int frame_idx = (int)sp_candidate->get_frame_idx_end();
									frame_idx >= (int)sp_candidate->get_frame_idx_start();
									--frame_idx)
								{
									// Out-of-bounds check not required because all frames are within a single screencast
									auto layer_access = sp_candidate->get_layer_access(frame_idx);
									sp_stable->push_blind_frame(layer_access, true);
								}
	
								merged = true;
								r_touched_left = true;
							}
							else if(!r_touched_right && stable_end == candidate_prev_frame) // candidate is directly before stable and stable not yet touched on that side in this iteration
							{
								// Push frames of candidate to stable
								for(
									int frame_idx = (int)sp_candidate->get_frame_idx_start();
									frame_idx <= (int)sp_candidate->get_frame_idx_end();
									++frame_idx)
								{
									// Out-of-bounds check not required because all frames are within a single screencast
									auto layer_access = sp_candidate->get_layer_access(frame_idx);
									sp_stable->push_blind_frame(layer_access, false);
								}

								merged = true;
								r_touched_right = true;
							}

							// Clean up after the merging
							if(merged)
							{
								// Schedule candidate to be removed
								candidates_to_be_removed.push_back(candidate_idx);
								
								// Add candidate to visual debug datum of stable
								VD(if(sp_stable_dates)
								{
									auto sp_candidate_datum = vd_datum("Merged Candidate");
									sp_candidate_datum->add(vd_matrices("Stitched Screenshot")
									->add(sp_candidate->get_stitched_screenshot()));
									sp_candidate_datum->add(vd_strings("frame_idx_start")
									->add(std::to_string(sp_candidate->get_frame_idx_start())));
									sp_candidate_datum->add(vd_strings("frame_idx_end")
									->add(std::to_string(sp_candidate->get_frame_idx_end())));
									(*sp_stable_dates)[stable_idx]->add(sp_candidate_datum);
								})
								
								break; // candidate has been merged, work on next candidate
							}
						}
					} // end of loop over candidates

					// Remove merged candidates
					for (auto idx : candidates_to_be_removed)
					{
						candidate_idxs.erase(idx);
					}
					intras_to_be_removed.insert(intras_to_be_removed.end(), candidates_to_be_removed.begin(), candidates_to_be_removed.end());
					
				} // end of iterations over one cluster
				
				// Put orphan candidates into visul debug datum
				VD(if(sp_orphan_candidates_datum)
				{
					for(int candidate_idx : candidate_idxs)
					{
						auto sp_candidate = rsp_cluster->at(candidate_idx);
						auto sp_candidate_datum = vd_datum("Candidate");
						sp_candidate_datum->add(vd_matrices("Stitched Screenshot")
						->add(sp_candidate->get_stitched_screenshot()));
						sp_candidate_datum->add(vd_strings("frame_idx_start")
						->add(std::to_string(sp_candidate->get_frame_idx_start())));
						sp_candidate_datum->add(vd_strings("frame_idx_end")
						->add(std::to_string(sp_candidate->get_frame_idx_end())));
						sp_orphan_candidates_datum->add(sp_candidate_datum);
					}
				})
				
				// Put merged stable states into the visual debug datum
				VD(
				if (sp_stable_dates && sp_merged_stable_datum)
				{
					// Go over map and put into datum
					for(auto& r_entry : *sp_stable_dates)
					{
						sp_merged_stable_datum->add(r_entry.second);
					}
				})
				
				// Remove intra-user states from cluster that have been moved into more stable ones
				std::sort(intras_to_be_removed.begin(), intras_to_be_removed.end());
				for (int i = (int)intras_to_be_removed.size() - 1; i >= 0; --i)
				{
					auto intra_idx = intras_to_be_removed.at(i);
					rsp_cluster->erase(rsp_cluster->begin() + intra_idx);
				}
			} // end of "go over clusters"

			// Put intra-user states from clusters back into the container
			for (auto& rsp_cluster : clusters)
			{
				for (auto sp_intra : *rsp_cluster)
				{
					_sp_container->push_back(sp_intra);
				}
			}

			// Move product out of work
			return _sp_container;
		}

		void Cleaner::internal_report(Cleaner::ReportType& r_report)
		{
			r_report.set_progress(1.f);
		}
	}
}
