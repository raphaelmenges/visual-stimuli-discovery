//! Log dates walker.
/*!
Log dates walker walks over log dates and serves corresponding data like layer and screenshot data.
*/

#pragma once

#include <Data/LogImage.hpp>
#include <Data/LogDatum.hpp>
#include <libsimplewebm.hpp>

namespace util
{
	// Struct to store how to access a layer via indices for general and the direct pointer for local processing
	struct LayerPack
	{
		LayerPack(std::vector<unsigned int> access, std::shared_ptr<const data::Layer> sptr) :
			access(access), sptr(sptr) {}

		std::vector<unsigned int> access; // empty for root
		std::shared_ptr<const data::Layer> sptr; // should be only used for local actions, not easy serializable
	};


	// Class to walk over log dates and corresponding log images in screencast
	class LogDatesWalker
	{
	public:

		// Constructor. On needs to walk one frame before values can be retrieved.
		// If no webm_path is provided, log images are not available
		LogDatesWalker(
			std::shared_ptr<data::LogDates_const> sp_log_dates, // processed datacast
			std::string webm_path = ""); // path to screencast

		// Walks one frame. Returns true when frame has been available, otherwise false
		bool step();

		// Get log image of last walked frame (nullptr before walk)
		std::shared_ptr<const data::LogImage> get_log_image() const;

		// Get log datum of last walked frame (nullptr before walk)
		std::shared_ptr<const data::LogDatum> get_log_datum() const;

		// Get layers to be processed for this frame
		std::vector<LayerPack> get_layer_packs() const;

		// Get current frame idx (-1 before walk)
		int get_frame_idx() const;

		// Get frame count
		unsigned int get_frame_count() const;

	private:

		// Members
		std::shared_ptr<data::LogDates_const> _sp_log_dates = nullptr;
		std::unique_ptr<simplewebm::VideoWalker> _up_video_walker = nullptr;
		const unsigned int _frame_count;

		// Members holding current values
		int _frame_idx = -1;
		std::shared_ptr<const data::LogDatum> _sp_log_datum = nullptr;
		std::shared_ptr<const data::LogImage> _sp_log_image = nullptr;
	};
}
