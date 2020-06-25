#pragma once

#include <Core/Core.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>

namespace data
{
	// State (common base of Intra- and Inter-User State)
	class State
	{
	public:

		// Typedefs (simple structures for input data to serialize)
		typedef std::vector<std::tuple<core::long64, int, int, std::string> > MouseData; // timestamp, x, y, type of the event
		typedef std::vector<std::tuple<core::long64, int, int> > GazeData; // timestamp, x, y
	
		// Constructor, takes matrix as reference (does not copy!)
		State(cv::Mat stitched_screenshot);
	
		// Making the class abstract
		virtual ~State() = 0;
		
		// Set stitched screenshot
		void set_stitched_screenshot(cv::Mat stitched_screenshot);
		
		// Get reference to stitched screenshot
		const cv::Mat get_stitched_screenshot() const;

		// Get reference to covered stitched screenshot.
		// Returns reference to covered rectangular part of the stitched screenshot matrix
		// TODO: This function is not threadsafe - but this is not required atm
		const cv::Mat get_covered_stitched_screenshot() const;
		
		// Get frame count of contained frames
		virtual unsigned int get_total_frame_count() const = 0;
		
		// Get count of contained sessions
		virtual unsigned int get_total_session_count() const = 0;
		
	private:
	
		// Delete copy constructor
		State(const State&) = delete;

		// Delete assignment constructor
		State& operator=(const State&) = delete;
	
		// Members
		cv::Mat _stitched_screenshot; // matrix with pixels of stitched screenshot

		// Covering rectangle (mutable because may be changed in getter const method)
		mutable cv::Rect _covered;
		mutable bool _update_covered_rect = false;
	};
}
