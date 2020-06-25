#include "Tuning.hpp"
#include <Core/Core.hpp>
#include <Util/LayerComparator.hpp>
#include <libsimplewebm.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include <set>

const int ORB_SCROLL_THRESHOLD = core::mt::get_config_value(225, { "processing", "tuning", "orb_scroll_threshold" });

namespace stage
{
	namespace processing
	{
		namespace tuning
		{
			Interface::~Interface() {}

			ORBscroll::ORBscroll(
				VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
				std::shared_ptr<const data::LogDatumContainer> sp_log_datum_container)
				:
				Interface(VD(sp_dump, ) sp_log_datum_container->get_session()->get_id()),
				_up_walker(std::unique_ptr<util::LogDatesWalker>(new util::LogDatesWalker(sp_log_datum_container->get(), sp_log_datum_container->get_session()->get_webm_path())))
			{
				_sp_container = std::shared_ptr<ProductType>(new ProductType(sp_log_datum_container->get_session(), sp_log_datum_container->get_datacast_duration()));
			}

			std::shared_ptr<ORBscroll::ProductType> ORBscroll::internal_step()
			{
				// Walk one frame in screencast and log dates
				if (_up_walker->step()) // another frame is available
				{
					// Store extended information about layers
					std::deque<std::shared_ptr<ExLayer> > ex_layers;

					// Retrieve all layers of that frame
					auto layer_packs = _up_walker->get_layer_packs();

					// Deep copy log datum (which is then tuned)
					auto up_log_datum = _up_walker->get_log_datum()->deep_copy();

					// Go over layers and add them to deque to be processed
					for (const auto& r_pack : layer_packs)
					{
						// Do skip fixed elements
						// if (r_pack.sptr->get_type() == data::LayerType::Fixed) { continue; } // TODO: remove later?

						// Extended layer automatically computes ORB features
						ex_layers.push_back(std::shared_ptr<ExLayer>(
							new ExLayer(
								_up_walker->get_log_image(), // log image of the frame
								up_log_datum->access_layer(r_pack.access) // get pointer to layer of deep copied log datum
							)));
					}

					// Makes only sense to estimate scrolling if this is not the first frame
					if (_up_walker->get_frame_idx() > 0)
					{
						// Match layers of this frame with layer of previous frame
						for (const auto sp_ex_layer : ex_layers) // current frame
						{
							for (const auto sp_prev_ex_layer : _prev_ex_layers) // previous frame
							{
								// TODO remove matched prev frame from deque? otherwise might different current layers might match with the same previous frame

								// Check similarity between layers across the two frames
								auto sp_prev_layer = sp_prev_ex_layer->_sp_layer;
								auto sp_layer = sp_ex_layer->_sp_layer; // layer in current frame
								float similarity_score =
									util::layer_comparator::compare(
										sp_prev_layer,
										sp_layer).value();
								if (similarity_score > core::mt::get_config_value(0.5f, { "model", "processing", "layer_threshold" }))
								{
									VD(
									std::shared_ptr<core::visual_debug::Datum> sp_datum = nullptr;
									if (_sp_dump)
									{
										sp_datum = vd_datum("Frame");
										sp_datum->add(vd_strings("Frame: ")->add(std::to_string(_up_walker->get_frame_idx())));
										sp_datum->add(vd_strings("Previous XPath: ")->add(sp_prev_layer->get_xpath()));
										sp_datum->add(vd_strings("Current XPath: ")->add(sp_layer->get_xpath()));
										_sp_dump->add(sp_datum);
									})
									int vd_original_scroll_y = 0;
									int vd_scroll_y = 0;

									// Similar enough, perform the ORB-based scrolling estimation
									float relative_scroll_x = 0.f;
									float relative_scroll_y = 0.f;
									bool success = estimate_relative_scrolling(
										VD(sp_datum, )
										sp_prev_ex_layer,
										sp_ex_layer,
										relative_scroll_x,
										relative_scroll_y);

									// Check scrolling from previous frame and apply absolute scrolling
									if(success)
									{
										// Store value for visual debugging
										vd_original_scroll_y = sp_layer->get_scroll_y();

										// Scrolling values
										int scroll_x = sp_prev_layer->get_scroll_x() + (int)std::round(relative_scroll_x);
										int scroll_y = sp_prev_layer->get_scroll_y() + (int)std::round(relative_scroll_y);

										// Compare to stored scrolling (for root layer, only, others are not available in datacast)
										if (sp_ex_layer->_sp_layer->get_type() == data::LayerType::Root)
										{
											int abs_diff_x = std::abs(scroll_x - sp_layer->get_scroll_x());
											int abs_diff_y = std::abs(scroll_y - sp_layer->get_scroll_y());
											if (abs_diff_x > ORB_SCROLL_THRESHOLD) // x-direction
											{
												scroll_x = sp_layer->get_scroll_x();
											}
											if (abs_diff_y > ORB_SCROLL_THRESHOLD) // y-direction
											{
												scroll_y = sp_layer->get_scroll_y();
											}
										}

										sp_layer->set_scroll_x(scroll_x);
										sp_layer->set_scroll_y(scroll_y);
										
										// Store value for visual debugging
										vd_scroll_y = scroll_y;
										
									} // else: leave scrolling as reported by datacast

									// Add some more info to the visual debug datum
									VD(
									if (sp_datum)
									{
										sp_datum->add(vd_strings("Frame Time: ")->add(std::to_string(up_log_datum->get_frame_time())));
										sp_datum->add(vd_strings("Original Y-Scrolling: ")->add(std::to_string(vd_original_scroll_y)));
										sp_datum->add(vd_strings("Y-Scrolling: ")->add(std::to_string(vd_scroll_y)));
									})

									break; // breaks for loop for this layer in the current frame
								}
							}
						}
					}

					// Store current ex layers into member as previous ex layers
					_prev_ex_layers = ex_layers;

					// Store tuned log datum in product
					_sp_container->push_back(std::move(up_log_datum));

					return nullptr; // not yet done
				}
				else // no further frame available
				{
					return _sp_container;
				}
			}

			void ORBscroll::internal_report(ReportType& r_report)
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

			bool ORBscroll::estimate_relative_scrolling(
				VD(std::shared_ptr<core::visual_debug::Datum> sp_datum, )
				std::shared_ptr<const ORBscroll::ExLayer> sp_prev,
				std::shared_ptr<const ORBscroll::ExLayer> sp_current,
				float& r_scroll_x,
				float& r_scroll_y) const
			{
				if (sp_prev->_descriptors.empty() || sp_current->_descriptors.empty())
				{
					return false;
				}
				bool success = false;

				// TODO: implement x-scrolling

				// Brute force matcher
				cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_HAMMING);
				std::vector<cv::DMatch> matches;
				matcher->match(sp_prev->_descriptors, sp_current->_descriptors, matches);

				// Below not required as fixed elements are treated within separated layers
				// Filter for features which are on the same viewport position and similar for both frames
				/*
				std::set<int> to_delete;
				for (int i = 0; i < (int)matches.size(); i++)
				{
					const auto& r_match = matches.at(i);
					int index_0 = r_match.queryIdx;
					int index_1 = r_match.trainIdx;
					if (r_match.distance <= 1 // similarity of descriptors
						&& (core::math::euclidean_dist(sp_prev->_keypoints.at(index_0).pt, sp_current->_keypoints.at(index_1).pt) < 2.f)) // spatial similarity
					{
						to_delete.emplace(i);
					}
				}

				// Remove to_delete matches
				for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it)
				{
					matches.erase(matches.begin() + *it);
				}
				*/

				// Sort matches
				std::sort(matches.begin(), matches.end()); // sort predicate is given by OpenCV

				// Only get first n matches
				/*
				unsigned int new_size = std::min(1000u, (unsigned int)matches.size());
				matches.resize(new_size);
				*/

				// Filter only for relevant matches
				matches.erase(std::remove_if(matches.begin(), matches.end(), [&](const cv::DMatch& r_match) -> bool { return r_match.distance > 10; }), matches.end());

				// Average distance of well matched keypoints
				/*
				if (!matches.empty())
				{
					for (const auto& r_match : matches)
					{
						int index_0 = r_match.queryIdx;
						int index_1 = r_match.trainIdx;
						estimated_scrolling_delta += (double)(r_previous_log_datum._keypoints.at(index_0).pt.y - r_log_datum._keypoints.at(index_1).pt.y); // it seems that the origin is in the upper left corner. Features move up when somebody scrolls down!
					}
					estimated_scrolling_delta /= (double)matches.size(); // TODO: average is not best idea: rather use supporters etc.
				}
				*/

				// Calculate homography using RANSAC to estimate transformation between frames
				std::vector<cv::Point2f> prev_matched_keypoints, cur_matched_keypoints;
				if (!matches.empty())
				{
					// Retrieve keypoints of matches
					for (const auto& r_match : matches)
					{
						int index_0 = r_match.queryIdx;
						int index_1 = r_match.trainIdx;
						prev_matched_keypoints.push_back(sp_prev->_keypoints.at(index_0).pt);
						cur_matched_keypoints.push_back(sp_current->_keypoints.at(index_1).pt);
					}

					// Computation of homography
					cv::Mat H = cv::findHomography(prev_matched_keypoints, cur_matched_keypoints, cv::RANSAC);

					// Extract y-scrolling
					if (!H.empty()) // if no match found is empty
					{
						r_scroll_y = -(float)H.at<double>(1, 2);
						success = true;
					}
				}

				// Add information to visual debug dump
				VD(if (sp_datum) {

					// TODO: this viewport rect stuff seems broken
					auto sp_matrices = vd_matrices("Previous and current pixel data of the layer and matched keypoints");

					// Previous ex layer
					std::vector<cv::Point2i> int_prev_matched_keypoints;
					for (const auto& r_keypoint : prev_matched_keypoints) { int_prev_matched_keypoints.push_back(r_keypoint); }
					const cv::Mat prev_layer_mask = sp_prev->_sp_layer->get_view_mask();
					sp_matrices->add(sp_prev->_sp_image->get_layer_pixels(prev_layer_mask), int_prev_matched_keypoints);

					// Current ex layer
					std::vector<cv::Point2i> int_cur_matched_keypoints;
					for (const auto& r_keypoint : cur_matched_keypoints) { int_cur_matched_keypoints.push_back(r_keypoint); }
					const cv::Mat current_layer_mask = sp_current->_sp_layer->get_view_mask();
					sp_matrices->add(sp_current->_sp_image->get_layer_pixels(current_layer_mask), int_cur_matched_keypoints);

					sp_datum->add(sp_matrices);

					// Print estimated scrolling offset
					sp_datum->add(vd_strings("Estimated y-scrolling offset")->add(std::to_string(r_scroll_y)));
					
				})
				
				return success;
			}

			ORBscroll::ExLayer::ExLayer(
				std::shared_ptr<const data::LogImage> sp_image,
				std::shared_ptr<data::Layer> sp_layer) :
				_sp_image(sp_image), _sp_layer(sp_layer)
			{
				// TODO issue: either create once features for complete screenshot and then have problems with layers influencing each other
				// or do it per layer and mask other layers from screenshot

				// Create gray clone of viewport image
				const cv::Mat& gray = sp_image->get_viewport_pixels_gray();

				// Detect ORB keypoints
				const int CELL_COUNT_X = 4; // cell count
				const int CELL_COUNT_Y = 3;
				int cell_size_x = gray.size().width / CELL_COUNT_X;
				int cell_size_y = gray.size().height / CELL_COUNT_Y;
				cv::Ptr<cv::FeatureDetector> detector = cv::ORB::create(1000 / (CELL_COUNT_X * CELL_COUNT_Y)); // make ~1000 keypoints in total
				_keypoints.clear();
				const cv::Mat viewport_layer_mask = sp_layer->get_view_mask();
				for (int y = 0; y < CELL_COUNT_Y; y++)
				{
					for (int x = 0; x < CELL_COUNT_X; x++)
					{
						std::vector<cv::KeyPoint> keypoints;
						cv::Mat mask(gray.size(), CV_8UC1, cv::Scalar(0)); // create empty mask
						cv::Rect rect(x * cell_size_x, y * cell_size_y, cell_size_x, cell_size_y);
						viewport_layer_mask(rect).copyTo(mask(rect)); // TODO: strong assumption, that that mask (coming from image) and viewport_layer_mask (coming from combination of image and datacast) have the same extent?
						detector->detect(gray, keypoints, mask);
						_keypoints.insert(_keypoints.end(), keypoints.begin(), keypoints.end());

						/*
						std::string window_name = "window-" + std::to_string(x) + "-" + std::to_string(y);
						cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
						// cv::imshow(window_name, sp_intra_user_state->get_covered_stitched_screenshot());
						cv::imshow(window_name, mask_area);
						cv::waitKey(0);
						cv::destroyWindow(window_name);
						*/
					}
				}

				// Compute descriptors
				if (!_keypoints.empty())
				{
					auto extractor = cv::ORB::create((int)_keypoints.size());
					extractor->compute(gray, _keypoints, _descriptors);
				}

				/*
				// Manual debugging
				cv::Mat viewport = sp_image->get_visible_viewport_pixels().clone();
				for (const auto& r_keypoint : _keypoints)
				{
					// Circle keypoint
					cv::circle(
						viewport,
						r_keypoint.pt,
						2, cv::Scalar(0, 0, 0, 255), 2, 8, 0);

					// Draw exact keypoint
					cv::Vec4b& r_color = viewport.at<cv::Vec4b>(r_keypoint.pt);
					r_color[0] = 0;
					r_color[1] = 0;
					r_color[2] = 255;
					r_color[4] = 255;
				}


				std::string window_name = "window-" + sp_layer->get_xpath();
				cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
				// cv::imshow(window_name, sp_intra_user_state->get_covered_stitched_screenshot());
				cv::imshow(window_name, viewport);
				cv::waitKey(0);
				cv::destroyWindow(window_name);
				*/
				
				// Compute intra similarity (Web page might have repetive elements which will confuse features)
				cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_HAMMING);
				std::vector<std::vector<cv::DMatch> > matches;
				matcher->knnMatch(_descriptors, _descriptors, matches, 2); // k = 1 would only output similarity with itself

				// Throw away keypoints which descriptors are matching too good (similar within image)
				std::set<int> to_delete;
				for (const auto& r_match : matches)
				{
					for (const auto& r_inner_match : r_match)
					{
						if ((r_inner_match.queryIdx != r_inner_match.trainIdx) && r_inner_match.distance < 5)
						{
							to_delete.emplace(r_inner_match.queryIdx);
							to_delete.emplace(r_inner_match.trainIdx);
						}
					}
				}

				// Remove all too good keypoints (back to front)
				for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it)
				{
					_keypoints.erase(_keypoints.begin() + *it);
				}

				// Recompute descriptors (probably not required, one could remove descriptor rows according to deleted keypoints)
				_descriptors = cv::Mat();
				if (!_keypoints.empty())
				{
					auto extractor = cv::ORB::create((int)_keypoints.size());
					extractor->compute(gray, _keypoints, _descriptors);
				}
			}
		}
	}
}
