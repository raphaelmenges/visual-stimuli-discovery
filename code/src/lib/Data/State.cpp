#include "Data/State.hpp"

namespace data
{
	State::State(cv::Mat stitched_screenshot) : _stitched_screenshot(stitched_screenshot)
	{
		// Remember to update covered rect as stitched screenshot has been updated
		_update_covered_rect = true;
	}
	
	State::~State() {}
	
	void State::set_stitched_screenshot(cv::Mat stitched_screenshot)
	{
		_stitched_screenshot = stitched_screenshot;

		// Remember to update covered rect as stitched screenshot has been updated
		_update_covered_rect = true;
	}
	
	const cv::Mat State::get_stitched_screenshot() const
	{
		return _stitched_screenshot;
	}

	const cv::Mat State::get_covered_stitched_screenshot() const
	{
		// Update covered rect if required (TODO: mut const looks dangerous for multi-threading)
		if (_update_covered_rect)
		{
			_covered = core::opencv::covering_rect_bgra(_stitched_screenshot);
			_update_covered_rect = false;
		}

		return _stitched_screenshot(_covered);
	}
}
