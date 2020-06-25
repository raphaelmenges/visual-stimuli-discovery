#include "Parser.hpp"
#include <Core/Core.hpp>
#include <opencv2/opencv.hpp>
#include <libsimplewebm.hpp>
#include <fstream>
#include <deque>

const float TIME_BIAS_DATACAST = core::mt::get_config_value(0.0f, { "processing", "parser", "time_bias_datacast" });

namespace stage
{
	namespace processing
	{
		namespace parser
		{
			Interface::~Interface() {}

			LogRecord::LogRecord(
				VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
				std::shared_ptr<const data::Session> sp_session)
				:
				Interface(VD(sp_dump, ) sp_session->get_id()),
				_webm_path(sp_session->get_webm_path())
			{
				// Open datacast
				std::ifstream json_file(sp_session->get_json_path()); // TODO: check whether file exists and return error if not
				json_file >> _datacast;

				// Open screencast
				auto up_dry_video_walker = simplewebm::create_video_walker(_webm_path);

				// Perform a dry walk on the complete video to gather times
				up_dry_video_walker->dry_walk(_sp_times, 0); // TODO: check status
				_frame_count = (unsigned int)_sp_times->size();
				int frame_limit = sp_session->get_frame_limit();
				if (frame_limit >= 0)
				{
					_frame_count = std::min(_frame_count, (unsigned int)frame_limit);
				}

				// Prepare events phase
				_events = _datacast.at("Events"); // TODO: check for exception
				_it_events = _events.begin(); // might be already end() if empty

				// Prepare layers phase
				_layers = _datacast.at("Layers"); // TODO: check for exception
				_it_layers = _layers.begin(); // might be already end() if empty

				// Prepare input phase (mouse data also stored under events in .json)
				_mouse = _datacast.at("Events"); // TODO: check for exception
				_it_mouse = _mouse.begin(); // might be already end() if empty
				_gaze = _datacast.at("Gaze");
				_it_gaze = _gaze.begin();

				// Retrieve duration of datacast and duration of one frame
				core::long64 startMS = 0, endMS = 0;
				for(auto& r_item : _datacast.at("Infos"))
				{
					std::string type = r_item.at("type");
					if(type == "videoStarted")
					{
						std::string ts_string = r_item["qtGlobalTs"].get<std::string>();
						startMS = std::stoll(ts_string);
					}
					else if(type == "videoEnded")
					{
						std::string ts_string = r_item["qtGlobalTs"].get<std::string>();
						endMS = std::stoll(ts_string);
					}
					else if(type == "meta")
					{
						std::string rate_string = r_item["videoFramerate"].get<std::string>();
						int rate = std::stoi(rate_string);
						_frame_duration = 1000 / rate;
						
					}
				}

				// Get times where the document changes (is hidden)
				auto states = _datacast.at("States");
				auto it_states = states.begin();
				while(it_states != states.end())
				{
					auto event = it_states.value();
					if (event["type"].get<std::string>() == "documentIsHidden")
					{
						document_change_times_ms.push_back(event["qtVideoTs"].get<core::long64>());
					}
					++it_states;
				}

				// Create product
				_sp_container = std::shared_ptr<ProductType>(new ProductType(sp_session, ((double)(endMS-startMS)) / 1000.0));
			}

			std::shared_ptr<LogRecord::ProductType> LogRecord::internal_step()
			{
				bool phase_done = false;
				switch (_phase)
				{
				case Phase::Events:
					phase_done = parse_events();
					if (phase_done) { _phase = Phase::Layers; } // next phase
					break;
				case Phase::Layers:
					phase_done = parse_layers();
					if (phase_done) { _phase = Phase::Input; } // next phase
					break;
				case Phase::Input:
					phase_done = parse_input();
					if (phase_done)
					{
						// Before returning the product, fill the visual debug dump
						VD(if (_sp_dump) {

							// Walk over video in parallel and store the screenshots for nice visual debugging
							auto up_video_walker = simplewebm::create_video_walker(_webm_path);

							// Go over log dates and push visual debug dates into the dump
							for (const auto& r_log_datum : *_sp_container->get().get())
							{
								// Extract screenshot
								auto sp_images = std::shared_ptr<std::vector<simplewebm::Image> >(new std::vector<simplewebm::Image>);
								up_video_walker->walk(sp_images, 1); // TODO: check status
								cv::Mat screen_pixels;
								if (!sp_images->empty())
								{
									screen_pixels = cv::Mat(
										sp_images->at(0).height,
										sp_images->at(0).width,
										CV_8UC3,
										const_cast<char *>(sp_images->at(0).data.data()));
								}

								// Add visual debug datum of log datum and apppend corresponding screenshot
								auto sp_datum = r_log_datum->create_visual_debug_datum();
								sp_datum->add(vd_matrices("Screenshot")->add(screen_pixels));
								sp_datum->add(vd_strings("Root Scroll Y: ")->add(std::to_string(r_log_datum->get_root()->get_scroll_y())));
								_sp_dump->add(sp_datum);
							}
						})

						// Return product of parser when complete
						return _sp_container;
					}
					break;
				}
				return nullptr; // not yet done
			}

			void LogRecord::internal_report(LogRecord::ReportType& r_report)
			{
				// Just a rough guess: 50% is about events, the other 50% about fixed elements
				switch (_phase)
				{
				case Phase::Events:
				{
					float size = (float)_events.size();
					if (size > 0.f)
					{
						r_report.set_progress(((float)(_it_events - _events.begin()) / size) * 0.5f);
					}
					else
					{
						r_report.set_progress(0.5f);
					}
				} break;
				case Phase::Layers:
				{
					float size = (float)_layers.size();
					if (size > 0.f)
					{
						r_report.set_progress((((float)(_it_layers - _layers.begin()) / size) * 0.5f) + 0.5f);
					}
					else
					{
						r_report.set_progress(1.f);
					}
				} break;
				case Phase::Input:
				{
					r_report.set_progress(1.f);
				} break;
				}
			}

			bool LogRecord::parse_events()
			{
				// Walk over events in datacast until each frame time is reached
				if (_events_frame_idx < _frame_count)
				{
					// Read time of frame in screencast (seconds)
					double time = _sp_times->at(_events_frame_idx);
					time += TIME_BIAS_DATACAST;

					// Create or deep copy previous log datum
					std::shared_ptr<data::LogDatum> sp_log_datum = nullptr;
					if (_events_frame_idx == 0) // create
					{
						// Create initial log datum
						sp_log_datum = std::shared_ptr<data::LogDatum>(new data::LogDatum(time));

						// Parse for values that should be const over the complete time but still stored with timestamp
						auto tmp_it = _datacast.at("Events").begin();
						while (tmp_it != _datacast.at("Events").end())
						{
							auto event = tmp_it.value();
							if (event["type"].get<std::string>() == "webviewGeometry")
							{
								sp_log_datum->set_viewport_width(event["width"].get<int>());
								sp_log_datum->set_viewport_height(event["height"].get<int>());
								sp_log_datum->set_viewport_on_screen_position(
									cv::Point2i(event["x"].get<int>(), event["y"].get<int>()));
								break; // break after first occurence (if it will be looked for multiple values here, use bool instead)
							}
							++tmp_it;
						}
						sp_log_datum->set_viewport_pos(cv::Point2i(0, 0)); // as of now, screencast only contains viewport, no more desktop recording
					}
					else // deep copy of previous log datum
					{
						sp_log_datum = _sp_container->get()->at(_events_frame_idx - 1)->deep_copy(time);
					}

					// Check for document change (in order to reset scrolling at page change)
					if(!_sp_container->get()->empty())
					{
						std::shared_ptr<const data::LogDatum> sp_prev_log_datum = _sp_container->get()->back();
						core::long64 prev_time_ms = (core::long64)(sp_prev_log_datum->get_frame_time() * 1000);
						core::long64 curr_time_ms = (core::long64)(time * 1000);
						for(auto change_time_ms : document_change_times_ms)
						{
							change_time_ms += 200; // add time of one frame (change takes anyway up to a second and better not wrongly adjust last frame before change)
							if(prev_time_ms < change_time_ms && curr_time_ms >= change_time_ms)
							{
								sp_log_datum->get_root()->set_scroll_x(0);
								sp_log_datum->get_root()->set_scroll_y(0);
							}
						}
					}

					// Go through datacast until the end time of the frame to accomondate for incoming events
					bool until_time = true;
					while (until_time && _it_events != _events.end())
					{
						// Retrieve event from datacast and its time within in the screencast
						auto event = _it_events.value();
						core::long64 ms = event["qtVideoTs"].get<core::long64>(); // fallback: timestamp from Qt

						/*
						auto it_js_ms = event.find("jsVideoTs");
						if (it_js_ms != event.end())
						{
							ms = (*it_js_ms).get<core::long64>(); // timestamp from JavaScript
						}
						*/

						// Check whether event is still within time of frame
						if (ms <= (core::long64)(time * 1000.0))
						{
							std::string type = event["type"].get<std::string>();
							if (type == "jsScroll") // scrolling
							{
								sp_log_datum->get_root()->set_scroll_y(event["scrollY"].get<int>());
							}
							else if (type == "webviewGeometry") // webview geometry update (should not happen)
							{
								sp_log_datum->set_viewport_width(event["width"].get<int>());
								sp_log_datum->set_viewport_height(event["height"].get<int>());
								sp_log_datum->set_viewport_on_screen_position(
									cv::Point2i(event["x"].get<int>(), event["y"].get<int>()));
							}

							// TODO: overflow scrolling, maybe later video play and pause? -> need layer structures which are parsed after the events -> "layer_events" as extra step?

							// Next event in datacast
							++_it_events;
						}
						else
						{
							// Stop walking events for this log datum
							until_time = false;
						}
					}

					// Push log datum into vector of log dates
					_sp_container->push_back(sp_log_datum);

					// Increase index of frame
					++_events_frame_idx;
				}

				// Return true if all frames have been processed
				return _events_frame_idx >= _frame_count;
			}

			bool LogRecord::parse_layers()
			{
				// Check for sanity of iterator (never enters if container is empty)
				if (_it_layers != _layers.end())
				{
					// Times of layer the iterator points at
					auto layer = _it_layers.value();
					core::long64 ms_start = layer["qtVideoTs_first"].get<core::long64>();
					core::long64 ms_end = layer["qtVideoTs_last"].get<core::long64>();

					// Hotfix for start that is reported slightly later than the layer was actually visible)
					if (ms_start < (core::long64)(1000.0 * TIME_BIAS_DATACAST))
					{
						ms_start = 0;
					}

					// Get xpath
					std::string xpath = layer["xpath"].get<std::string>();
					
					// Filter certain xpaths (this is adapted to the dataset, please think at least about external file with those definitions)
					bool proceed = true;
					if(xpath == "html/body"
					|| xpath.find("html/body/div-2x-container/div/div/div-SHORTCUT_FOCUSABLE_DIV/div-2/div/") != std::string::npos // reddit
					|| xpath.find("html-ng-app/body/footer/div/div-0/a") != std::string::npos) // broken "go to top" button on kia
					{
						proceed = false;
					}

					// Do not allow layer with specific xpath (rather bug in Web page or logger)
					if (proceed)
					{
						// Check type of layer
						std::string type = layer["type"].get<std::string>();
						if (type == "fixed")
						{
							// Fixed elements are already in viewport pixel space
							int view_x = layer["x"].get<int>();
							int view_y = layer["y"].get<int>();
							int view_width = layer["width"].get<int>();
							int view_height = layer["height"].get<int>();
							int zindex = layer["z-index"].get<int>();

							// Go over log dates and append layers
							unsigned int frame_idx = 0;
							while (frame_idx < _frame_count)
							{
								// Read time of frame in screencast (seconds)
								double time = _sp_times->at(frame_idx);
								time += TIME_BIAS_DATACAST;
								core::long64 time_ms = core::long64(time * 1000.0);

								// Get reference to log datum
								auto sp_log_datum = _sp_container->get()->at(frame_idx); // TODO: would be more elegant to use frame time in log datum

								// Check whether time is within occurence of layer
								core::long64 frame_window_offset = (core::long64) (0.125 * _frame_duration); // assume frame covers some time window
								if (time_ms > (ms_start - frame_window_offset) && time_ms < (ms_end + frame_window_offset))
								{
									// Check that layer does not yet exist (overlapping times in datacast might cause layer to be added twice in same log record)
									// auto root = sp_log_datum->get_root();
									// std::deque<std::shared_ptr<const Layer> > layers_to_check;
									// TODO Use layer comparator etc.

									// Append layer to root of that log datum
									auto _sp_layer = data::Layer::create();
									_sp_layer->set_type(data::LayerType::Fixed);
									_sp_layer->set_xpath(xpath);
									_sp_layer->set_view_pos(cv::Point2i(view_x, view_y));
									_sp_layer->set_view_width(view_width);
									_sp_layer->set_view_height(view_height);
									_sp_layer->set_zindex(zindex);
									sp_log_datum->get_root()->append_child(_sp_layer);
								}

								// Increase index of frame
								++frame_idx;
							}
						} // TODO introduce more types of layers
					}

					// Next layer in datacast
					++_it_layers;
				}

				// Return true when all fixed elements have been handled
				return _it_layers == _layers.end();
			}

			bool LogRecord::parse_input()
			{
				// Go over frames
				for (unsigned int frame_idx = 0; frame_idx < _frame_count; ++frame_idx)
				{
					// Get log datum of the frame to feed input to
					auto sp_log_datum = _sp_container->get()->at(frame_idx);

					// Get times
					double time = _sp_times->at(frame_idx);

					// Inputs for that frame
					std::vector<std::shared_ptr<const data::CoordinateInput> > inputs;
					
					// Go through mouse data until the end time of the frame
					bool until_time = true;
					while (until_time && _it_mouse != _mouse.end())
					{
						// Retrieve mouse input from datacast and its time within in the screencast
						auto mouse = _it_mouse.value();
						core::long64 ms = mouse["qtVideoTs"].get<core::long64>(); // fallback: timestamp from Qt

						/*
						auto it_js_ms = event.find("jsVideoTs");
						if (it_js_ms != event.end())
						{
							ms = (*it_js_ms).get<core::long64>(); // timestamp from JavaScript
						}
						*/

						// Check whether event is still within time of frame
						if (ms <= (core::long64)(time * 1000.0))
						{
							std::string type = mouse["type"].get<std::string>();
							if (type == "move") // cursor movement
							{
								inputs.push_back(std::make_shared<data::MoveInput>(ms, mouse["x"].get<int>(), mouse["y"].get<int>()));
							}
							else if (type == "click") // cursor click
							{
								inputs.push_back(std::make_shared<data::ClickInput>(ms, mouse["x"].get<int>(), mouse["y"].get<int>()));
							}
							// Ignore items with other type

							// Next item in datacast
							++_it_mouse;
						}
						else
						{
							// Stop walking events for this log datum
							until_time = false;
						}
					}

					// Go through gaze data until the end time of the frame
					until_time = true;
					while (until_time && _it_gaze != _gaze.end())
					{
						// Retrieve gaze sample from datacast and its time within in the screencast
						auto gaze = _it_gaze.value();
						core::long64 ms = gaze["qtVideoTs"].get<core::long64>();

						// Check whether event is still within time of frame
						if (ms <= (core::long64)(time * 1000.0))
						{
							// Extract gaze sample
							bool valid = true;
							float x = -1.0;
							float y = -1.0;
							if (!(gaze["leftX"].is_null()))
							{
								x = gaze["leftX"].get<float>();
							}
							else { valid = false; }
							if (!(gaze["leftY"].is_null()))
							{
								y = gaze["leftY"].get<float>();
							}
							else { valid = false; }

							// Push back
							inputs.push_back(std::make_shared<data::GazeInput>(ms, (int)x, (int)y, valid));
							
							// Next item in datacast
							++_it_gaze;
						}
						else
						{
							// Stop walking events for this log datum
							until_time = false;
						}
					}

					// Put collected inputs into corresponding layers
					std::deque<std::shared_ptr<data::Layer> > queue;
					queue.push_back(sp_log_datum->get_root());
					while (!queue.empty())
					{
						// Get next layer
						auto sp_layer = queue.front();
						queue.pop_front();

						// Add children of that layer to the queue
						auto children = sp_layer->get_children();
						for (auto& rsp_child : children)
						{
							queue.push_back(rsp_child);
						}

						// Check whether coordinate is in the white part of the layers mask
						auto mask = sp_layer->get_view_mask();

						// Go over collected inputs
						for (auto sp_input : inputs)
						{
							// Get coordinate in viewport x and y
							int view_x = sp_input->get_view_x();
							int view_y = sp_input->get_view_y();

							// Check whether coordinate is within viewport
							if (view_x < 0 || view_y < 0 || view_x >= sp_log_datum->get_viewport_width() || view_y >= sp_log_datum->get_viewport_height())
							{
								continue; // throw away input (TODO: or add to root?)
							}
			
							// Check whether coordinate is on layer
							auto value = mask.at<uchar>(
								view_y,
								view_x);

							// Decide whether to add input to layer
							if (value > 0) // TODO: might be increased, but as of now, mask entries are anyway either 0 or 255
							{
								// Push back input into layer (if masks are not disjunct, input is put into multiple layers
								sp_layer->push_back_input(sp_input); // multiple layers might point to the SAME input object...
							}
						}
					}

					// Increase index of frame
					++frame_idx;
				}

				return true;
			}
		}
	}
}
