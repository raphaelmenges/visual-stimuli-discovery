#include "Merger.hpp"
#include <Stage/Merging/Model.hpp>
#include <ThreadPool.h>
#include <set>
#include <math.h>

const core::long64 MERGE_THRESHOLD = core::mt::get_config_value(1024, { "model", "merging", "merge_threshold" });
const int THREAD_COUNT = core::mt::get_config_value(4, { "model", "merging", "thread_count" });

namespace stage
{
	namespace merging
	{
		Merger::Merger(
			VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
			std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
			std::string id,
			std::shared_ptr<data::IntraUserStates_const> sp_intras)
			:
			Work(VD(sp_dump, ) core::PrintReport(id)),
			_sp_classifier(sp_classifier),
			_sp_intras(sp_intras)
		{
			// Create matrix to hold pairwise similarity
			unsigned int intras_count = (unsigned int)_sp_intras->size();
			_similarity_matrix.resize(intras_count, intras_count); // strictly upper triangle part is used, as similarity is symmetric

			// Initialize empty vector of inter-user states, so vector of nullptrs
			_sp_inters = std::make_shared<data::InterUserStates>(intras_count, nullptr); // as many as intra-user states in cluster, so indexing fits to the matrix and the cluster
		}

		std::shared_ptr<Merger::ProductType> Merger::internal_step()
		{
			switch (_phase)
			{
			case Phase::InitSimiliarityMatrix:
				if (init_similarity_matrix())
				{
					_phase = Phase::Merging;
				}
				break;
			case Phase::Merging:
				if (merge())
				{
					_phase = Phase::Finalize;
				}
				break;
			case Phase::Finalize:
				if (finalize())
				{
					_sp_container = std::make_shared<ProductType>();
					_sp_container->set(_sp_inters);
				}
				break;
			}
			return _sp_container;
		}

		void Merger::internal_report(ReportType& r_report)
		{
			switch (_phase)
			{
			case Phase::InitSimiliarityMatrix:
				r_report.set_progress(0.f);
				break;
			case Phase::Merging:
				r_report.set_progress((1.f - _last_min_merged_similarity) / (1.f - MERGE_THRESHOLD)); // TODO: no more working correctly, adapt!
				break;
			case Phase::Finalize:
				r_report.set_progress(1.f);
				break;
			}
		}

		bool Merger::init_similarity_matrix()
		{
			/////////////////////////////////////////////////
			/// Initial similarity matrix (multi-threaded!)
			/////////////////////////////////////////////////

			// Define work to do by the threads
			auto compute_entry = [](
				std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
				int i,
				int j,
				std::shared_ptr<data::IntraUserStates_const> sp_intras)
				-> core::long64
			{
				core::long64 similarity = -1;

				// Avoid comparison to itself
				if (i != j)
				{
					// Fetch both intra-user states
					auto& sp_intra_a = sp_intras->at(i);
					auto& sp_intra_b = sp_intras->at(j);
					
					// Log which states are processed
					core::mt::log_info(
						"Participant A: ", sp_intra_a->get_container().lock()->get_session()->get_id(), // actually, success of lock must be checked but yes
						" Shot A: ", sp_intra_a->get_idx_in_container());
					core::mt::log_info(
						"Participant B: ", sp_intra_b->get_container().lock()->get_session()->get_id(), // actually, success of lock must be checked but yes
						" Shot B: ", sp_intra_b->get_idx_in_container());

					// Compare both intra-user states
					similarity = model::compute(
						sp_classifier,
						sp_intra_a,
						sp_intra_b);
				}

				return similarity;
			};

			// Create a thread pool
			ThreadPool pool(THREAD_COUNT);

			// Compute entries of similarity matrix
			unsigned int intras_count = (unsigned int)_sp_intras->size();
			std::map<int, std::map<int, std::future<core::long64> > > similarity_entries; // store future results
			for (unsigned int i = 0; i < intras_count; ++i) // rows of matrix
			{
				for (unsigned int j = i; j < intras_count; ++j) // columns of matrix
				{
					similarity_entries[i][j] = pool.enqueue(compute_entry, _sp_classifier, i, j, _sp_intras);
				}
			}

			// After filling pool with all tasks, wait for results and enter them into the matrix
			// Comment: should wait in the same order as queue is created
			for (auto& r_row : similarity_entries)
			{
				for (auto& r_column : r_row.second)
				{
					// Wait for future to be computed (locks at the future and waits for it to be done)
					int i = r_row.first;
					int j = r_column.first;
					core::long64 similarity = r_column.second.get();

					// Check for NaN
					// if (isnan(similarity)) { continue; } // do not overlap. treat as non-similar

					// Enter it to similarity matrix
					_similarity_matrix(i, j) = similarity;
					_similarity_matrix(j, i) = similarity;

					// Tell user about it
					core::mt::log_info("Similarity Matrix: ", i, ", ", j, " entry ", similarity, " calculated!");
				}
			}

			return true;
		}

		bool Merger::merge()
		{
			/////////////////////////////////////////////////
			/// Merge states into inter-user states
			/////////////////////////////////////////////////

			// Get field with max value from matrix
			core::long64 max_val = -1;
			int max_i = 0, max_j = 0;
			for (int i = 0; i < _similarity_matrix.rows(); ++i)
			{
				for (int j = i; j < _similarity_matrix.cols(); ++j)
				{
					core::long64 value = _similarity_matrix(i, j);
					if (max_val < value)
					{
						max_val = value;
						max_i = i;
						max_j = j;
					}
				}
			}

			// Merge intra-user states into inter-user states
			if (max_val > MERGE_THRESHOLD)
			{
				core::mt::log_info("Merge : MaxLoc: ", max_i, ",", max_j, " with value: ", std::to_string(max_val));
				_last_min_merged_similarity = _last_min_merged_similarity > max_val ? max_val : _last_min_merged_similarity;

				// Check which states to merge
				int i = max_i; // row
				int j = max_j; // column

				// Fetch both states to merge. First check whether already inter-user states exist, otherwise use intra-user state
				std::shared_ptr<const data::State> sp_state_a = _sp_inters->at(i);
				std::shared_ptr<const data::State> sp_state_b = _sp_inters->at(j);
				if (!sp_state_a)
				{
					sp_state_a = _sp_intras->at(i);
				}
				if (!sp_state_b)
				{
					sp_state_b = _sp_intras->at(j);
				}

				// Fetch both stitched screenshots
				cv::Mat pixels_a = sp_state_a->get_stitched_screenshot();
				cv::Mat pixels_b = sp_state_b->get_stitched_screenshot();

				// Merge both stitched screenshots
				int width = std::max(pixels_a.cols, pixels_b.cols);
				int height = std::max(pixels_a.rows, pixels_b.rows);
				cv::Mat canvas = cv::Mat::zeros(height, width, CV_8UC4);
				cv::Mat canvas_a = canvas(cv::Rect(0, 0, pixels_a.cols, pixels_a.rows)); // ROI within common canvas
				cv::Mat canvas_b = canvas(cv::Rect(0, 0, pixels_b.cols, pixels_b.rows)); // ROI within common canvas
				core::opencv::blend(pixels_a, canvas_a, canvas_a);
				core::opencv::blend(pixels_b, canvas_b, canvas_b);

				// Update states
				std::set<int> similarity_updates = { i, j }; // remember for which states the similarity with all other states must be updated
				auto sp_inter_a = _sp_inters->at(i);
				auto sp_inter_b = _sp_inters->at(j);
				if (sp_inter_a && sp_inter_b)
				{
					// Both states are inter-user states. Merge!
					auto sp_inter_c = data::InterUserState::merge(sp_inter_a, sp_inter_b, canvas);

					// Update all other pointers that point to the two merged inter-user states
					for (int x = 0; x < (int)_sp_inters->size(); ++x)
					{
						if (x != i && x != j)
						{
							std::shared_ptr<data::InterUserState> entry = _sp_inters->at(x);
							if (entry &&
								(static_cast<void*>(entry.get()) == static_cast<void*>(sp_inter_a.get())
									|| static_cast<void*>(entry.get()) == static_cast<void*>(sp_inter_b.get()))
								)
							{
								// Do not compute similarity within the same inter-user state
								_similarity_matrix(x, i) = -1;
								_similarity_matrix(i, x) = -1;
								_similarity_matrix(x, j) = -1;
								_similarity_matrix(j, x) = -1;
								_sp_inters->at(x) = sp_inter_c;

								// Update similiarities between the updated inter-user state and the other states
								similarity_updates.emplace(x);
							}
						}
					}

					// Overwrite the two inter-user states that have been merged
					_sp_inters->at(i) = sp_inter_c;
					_sp_inters->at(j) = sp_inter_c;
				}
				else if (sp_inter_a)
				{
					// One is inter-user state. Merge into that!
					auto sp_intra_b = _sp_intras->at(j);
					sp_inter_a->add_state(sp_intra_b, canvas); // overwrite stitched screenshot
					_sp_inters->at(j) = sp_inter_a;
				}
				else if (sp_inter_b)
				{
					// The other is inter-user state. Merge into that!
					auto sp_intra_a = _sp_intras->at(i);
					sp_inter_b->add_state(sp_intra_a, canvas); // overwrite stitched screenshot
					_sp_inters->at(i) = sp_inter_b;
				}
				else // both states at the indices are still intra-user states
				{
					// None is inter-user state. Merge both intra-user states into inter-user state one!
					auto sp_intra_a = _sp_intras->at(i);
					auto sp_intra_b = _sp_intras->at(j);
					auto intra_vec = { sp_intra_a, sp_intra_b };
					auto sp_inter_c = std::make_shared<data::InterUserState>(intra_vec, canvas);
					_sp_inters->at(i) = sp_inter_c;
					_sp_inters->at(j) = sp_inter_c;
				}

				// Mark merged states in similiarity matrix with -1
				_similarity_matrix(i, j) = -1;
				_similarity_matrix(j, i) = -1;
				
				/////////////////////////////////////////////////
				/// Update similarity matrix (multi-threaded!)
				/////////////////////////////////////////////////

				// Collect entries to update
				std::vector<std::pair<int, int> > entries_to_update;
				for (int k : similarity_updates) // go over changed states that require updated similarity
				{
					// Go over potential states to compute similarity to
					for (int l = 0; l < (int)_sp_inters->size(); ++l)
					{
						// Do not update if already merged, marked with negative value
						if (_similarity_matrix(l, k) <= 0) { continue; } // TODO: think about this indexing!

						// Comparsion to itself also not of interest (should be covered by aboves <= 0 check, anyway)
						if (k == l) { continue; }

						// Check whether other direction is already contained
						for (const auto& r_entry : entries_to_update)
						{
							if (r_entry.first == k && r_entry.second == l) { continue; }
						}

						// Put entry into vector
						entries_to_update.push_back({ k, l });
					}
				}

				// Define cache (inter-user states might be multiple times in matrix, thus, similarity might be computed only once between two of them)
				std::vector<std::tuple<void const *, void const *, core::long64> > similiarity_cache; // two states plus similarity score
				std::mutex cache_mutex;

				// Define function to be executed by thread
				auto update_entry = [&](
					std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
					int k,
					int l,
					std::shared_ptr<data::IntraUserStates_const> sp_intras,
					std::shared_ptr<const data::InterUserStates> sp_inters)
					-> core::long64
				{
					core::long64 similarity = 0;

					// Fetch both states
					std::shared_ptr<const data::State> sp_inner_a = sp_inters->at(k);
					std::shared_ptr<const data::State> sp_inner_b = sp_inters->at(l);
					if (!sp_inner_a)
					{
						sp_inner_a = sp_intras->at(k);
					}
					if (!sp_inner_b)
					{
						sp_inner_b = sp_intras->at(l);
					}

					// Check cache
					bool cache_used = false;
					{
						std::lock_guard<std::mutex> lock_guard(cache_mutex);
						for (auto item : similiarity_cache)
						{
							void const * inner_a_void = static_cast<void const *>(sp_inner_a.get());
							void const * inner_b_void = static_cast<void const *>(sp_inner_b.get());
							void const * cache_a_void = std::get<0>(item);
							void const * cache_b_void = std::get<1>(item);
							if ((inner_a_void == cache_a_void && inner_b_void == cache_b_void)
								|| (inner_b_void == cache_a_void && inner_a_void == cache_b_void))
							{
								similarity = std::get<2>(item);
								cache_used = true;
								break;
							}
						}
					}

					// If check of cache had no hit, compute it
					if (!cache_used)
					{
						// Compare both states
						similarity = model::compute(
							sp_classifier,
							sp_inner_a,
							sp_inner_b);

						// Check for NaN
						// if (isnan(similarity)) { similarity = 0; } // do not overlap. treat as non-similar

						// Update cache
						{
							std::lock_guard<std::mutex> lock_guard(cache_mutex);
							similiarity_cache.push_back({ sp_inner_a.get(), sp_inner_b.get(), similarity });
						}
					}

					return similarity;
				};

				// Create a thread pool
				ThreadPool pool(THREAD_COUNT);

				// Enqueue work into thread pool
				std::map<int, std::map<int, std::future<core::long64> > > similarity_entries; // store future results
				for (const auto& r_entry : entries_to_update)
				{
					similarity_entries[r_entry.first][r_entry.second] = pool.enqueue(update_entry, _sp_classifier, r_entry.first, r_entry.second, _sp_intras, _sp_inters);
				}

				// After filling pool with all tasks, wait for results and enter them into the matrix
				// Comment: should wait in the same order as queue is created
				for (auto& r_outer : similarity_entries)
				{
					for (auto& r_inner : r_outer.second)
					{
						// Wait for future to be computed (locks at the future and waits for it to be done)
						int k = r_outer.first;
						int l = r_inner.first;
						core::long64 similarity = r_inner.second.get();

						// Check for NaN
						// if (isnan(similarity)) { continue; } // do not overlap. treat as non-similar

						// Enter it to similarity matrix
						_similarity_matrix(k, l) = similarity;
						_similarity_matrix(l, k) = similarity;
					}
				}

				return false; // continue with merging
			}
			else
			{
				return true; // done with merging
			}
		}

		bool Merger::finalize()
		{
			///////////////////////////////////////////////////////////////
			/// Finalize inter-user states of the current layer cluster
			///////////////////////////////////////////////////////////////

			// Transfer remaining intra-user states and clean from pointers pointing to the same inter-user state
			std::vector<int> to_delete;
			for (int i = 0; i < (int)_sp_inters->size(); ++i)
			{
				auto curr = _sp_inters->at(i);
				if (curr == nullptr) // no inter-user state on that index
				{
					// Store intra-user state as inter-user state
					auto intra_vec = { _sp_intras->at(i) };
					_sp_inters->at(i) =
						std::make_shared<data::InterUserState>
							(intra_vec, // list of intra-user states covered by the inter-user state
							_sp_intras->at(i)->get_stitched_screenshot().clone()); // stitched screenshot for the inter-user state
				}
				else // there is a inter-user state at that index, chech whether it is unique. If not, delete it
				{
					for (int j = 0; j < i; ++j)
					{
						auto prev = _sp_inters->at(j);
						if (prev) // not nullptr
						{
							if (static_cast<void const *>(curr.get()) == static_cast<void const *>(prev.get()))
							{
								// Delete duplicate
								to_delete.push_back(i);
								break; // inner for loop
							}
						}
					}
				}
			}

			// Actual deletion of inter-user state pointers
			for (int i = (int)to_delete.size() - 1; i >= 0; --i)
			{
				_sp_inters->erase(_sp_inters->begin() + to_delete.at(i));
			}

			// Push inter-user states into visual debug dump
			VD(if (_sp_dump)
			{
				// Add all inter-user states
				for (const auto& rsp_state : *_sp_inters.get())
				{
					// Prepare values
					auto intras = rsp_state->get_states();
					int intras_count = (int)intras.size();
					int total_frame_count = 0;
					std::map<std::string, std::pair<int, int> > session_intra_and_frame_count;
					for (const auto& r_intra : intras)
					{
						int frame_count = ((int)r_intra->get_frame_idx_end() - (int)r_intra->get_frame_idx_start()) + 1;
						auto id = r_intra->get_container().lock()->get_session()->get_id();
						auto it = session_intra_and_frame_count.find(id);
						if (it != session_intra_and_frame_count.end())
						{
							(*it).second.first += 1; // intra count
							(*it).second.second += frame_count; // frame count
						}
						else
						{
							session_intra_and_frame_count[id] = { 1, frame_count };
						}
						total_frame_count += frame_count;
					}

					// Create entry for inter-user state
					auto sp_state = vd_datum("Inter-User State");
					_sp_dump->add(sp_state);

					// Add merged screenshot
					sp_state->add(vd_matrices("Merged Screenshot")->add(rsp_state->get_stitched_screenshot()));

					// Add info
					auto sp_info = vd_strings("Info");
					sp_state->add(sp_info);
					sp_info->add("Total Intra Count: " + std::to_string(intras_count) + ";");
					sp_info->add("Total Frame Count: " + std::to_string(total_frame_count));

					// Add more detailed info
					auto sp_details = vd_strings("Details");
					sp_state->add(sp_details);
					for (auto const& intra_and_frame_count : session_intra_and_frame_count)
					{
						sp_details->add("Session: " + intra_and_frame_count.first + ";");
						sp_details->add("Intra Count: " + std::to_string(intra_and_frame_count.second.first) + ";");
						sp_details->add("Frame Count: " + std::to_string(intra_and_frame_count.second.second));
					}

				}
			})

			return true;
		}
	}
}
