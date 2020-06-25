#include "LogDatesWalker.hpp"
#include <deque>

namespace util
{
	LogDatesWalker::LogDatesWalker(
		std::shared_ptr<data::LogDates_const> sp_log_dates,
		std::string webm_path)
		:
		_sp_log_dates(sp_log_dates), // log dates
		_frame_count((unsigned int) sp_log_dates->size()) // frame count is set to number of log dates (might be limited by user)
	{
		if (!webm_path.empty())
		{
			_up_video_walker = simplewebm::create_video_walker(webm_path); // video walker to extract screenshots
		}
	}

	bool LogDatesWalker::step()
	{
		int frame_idx = _frame_idx + 1;
		if (frame_idx < (int) _frame_count)
		{
			// Fetch log datum
			_sp_log_datum = _sp_log_dates->at(frame_idx);

			// Fetch log image
			if (_up_video_walker)
			{
				// Fetch screenshot of that frame from screencast
				auto sp_images = std::shared_ptr<std::vector<simplewebm::Image> >(new std::vector<simplewebm::Image>);
				_up_video_walker->walk(sp_images, 1);

				// Create log image
				_sp_log_image = std::shared_ptr<data::LogImage>(new data::LogImage(sp_images->at(0), _sp_log_datum));
			}

			// Take over frame_idx
			_frame_idx = frame_idx;

			return true; // next frame has been available
		}
		else
		{
			return false; // already reached end of frames
		}
	}

	std::shared_ptr<const data::LogImage> LogDatesWalker::get_log_image() const
	{
		return _sp_log_image;
	}

	std::shared_ptr<const data::LogDatum> LogDatesWalker::get_log_datum() const
	{
		return _sp_log_datum;
	}

	std::vector<LayerPack> LogDatesWalker::get_layer_packs() const
	{
		// Determine how to access layers of that frame
		std::vector<LayerPack> layer_packs;
		if(_sp_log_datum != nullptr)
		{
			std::deque<LayerPack> layers_to_collect;
			layers_to_collect.push_back(LayerPack({}, _sp_log_datum->get_root())); // push back root layer
			while (!layers_to_collect.empty())
			{
				// Get layer pack from the front of the queue
				auto pack = layers_to_collect.front();
				layers_to_collect.pop_front();

				// Go over children of the front layer and add them to the queue
				auto child_count = pack.sptr->get_child_count();
				for (unsigned int child_idx = 0; child_idx < child_count; ++child_idx)
				{
					// Push back layers to process
					LayerPack child_pack(
						pack.access, // access indices
						pack.sptr->get_child(child_idx)); // pointer
					child_pack.access.push_back(child_idx); // push back index how to access that child
					layers_to_collect.push_back(child_pack);
				}

				// Add pop'd layer to layers to output
				layer_packs.push_back(pack);
			}
		}
		return layer_packs;
	}

	int LogDatesWalker::get_frame_idx() const
	{
		return _frame_idx;
	}

	unsigned int LogDatesWalker::get_frame_count() const
	{
		return _frame_count;
	}
}
