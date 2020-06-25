//! OCR descriptor.
/*!
Just stores OCR observations of an inputted image.
*/

#pragma once

#include <Util/OCREngine.hpp>
#include <opencv2/core/types.hpp>

namespace descriptor
{
	// OCR descriptor
	class OCR
	{
	public:

		// OCR box
		class Box
		{
		public:

			Box(std::string text, int confidence) : _text(text), _confidence(confidence) {};
			std::string get_text() const { return _text; }
			int get_confidence() const { return _confidence; }

		private:

			// Members
			std::string _text;
			int _confidence;
		};

		// Constructor
		OCR(std::shared_ptr<const cv::Mat> sp_image);

		// Get all words
		std::shared_ptr<const std::vector<std::string> > get_words() const { return _sp_words; }

		// Get boxes
		std::shared_ptr<const std::vector<Box> > get_boxes() const { return _sp_boxes; }

	private:

		// Members
		std::shared_ptr<std::vector<Box> > _sp_boxes = std::make_shared<std::vector<Box> >();
		std::shared_ptr<std::vector<std::string> > _sp_words = std::make_shared<std::vector<std::string> > ();
	};
}
