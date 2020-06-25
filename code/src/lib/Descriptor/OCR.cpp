#include "OCR.hpp"

#include <Core/Core.hpp>
#include <opencv2/opencv.hpp>
#include <Util/OCREngine.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

namespace descriptor
{
	// Expects 8bit BGRA image
	OCR::OCR(std::shared_ptr<const cv::Mat> sp_image)
	{
		// Convert input image to grayscale
		cv::Mat tess_input;
		core::opencv::BGRA2Y(*sp_image, tess_input);

		// Create OCR engine
		auto sp_OCR_engine = std::make_shared<util::OCREngine>();
		
		// Set provided image
		sp_OCR_engine->get_api()->SetImage(tess_input.data, tess_input.cols, tess_input.rows, 1, 1 * tess_input.cols);

		// Perform optical character recognition. Check returned value. Sometimes tesseract seems to crash and returns nullptr.
		Boxa* boxes = sp_OCR_engine->get_api()->GetComponentImages(tesseract::RIL_TEXTLINE, true, NULL, NULL);
		if (boxes)
		{
			for (int i = 0; i < boxes->n; ++i) // go over boxes of found components
			{
				BOX* box = boxaGetBox(boxes, i, L_CLONE);
				sp_OCR_engine->get_api()->SetRectangle(box->x, box->y, box->w, box->h); // sets rectangle for OCR
				const char* text = sp_OCR_engine->get_api()->GetUTF8Text(); // retrieve text from box
				int conf = sp_OCR_engine->get_api()->MeanTextConf(); // retrieve confidence of OCR (between 0 and 100)

				// Check configuration to withdraw recognitions with too low confidence
				if (((float)conf / 100.f) < core::mt::get_config_value(0.5f, { "descriptor", "OCR", "confidence_threshold" }))
				{
					// Tesseract has reserved memory for text which must be freed
					delete[] text;

					// Continue with next candidate
					continue;
				}

				// Save box
				_sp_boxes->push_back(Box(text, conf));

				// Add words to general collection of words
				std::istringstream iss(text);
				for (std::string s; iss >> s; )
				{
					if (!s.empty())
					{
						_sp_words->push_back(s);
					}
				}

				// Tesseract has reserved memory for text which must be freed
				delete[] text;
			}
		}
	}
}
