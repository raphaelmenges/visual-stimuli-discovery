//! Minimalistic wrapper of OCR engine.
/*!
Very simple wrapper of Tesseract OCR engine. Ensures uniform setup and a litte bit about cleanup.
One even has to include tesseract/baseapi.h and leptonica/allheaders.h when the API shall be used.
*/

#pragma once

#include <memory>

// Forward declaration
namespace tesseract
{
	class TessBaseAPI;
}

namespace util
{
	class OCREngine
	{
	public:

		// OCR engine mode
		enum class OEM {
			DEPRECATED, LSTM
		};

		// Constructor, creates API
		OCREngine(OEM oem = OEM::LSTM);

		// Destructor, cleanup of API
		virtual ~OCREngine();

		// Access to API. Very raw, use with caution
		tesseract::TessBaseAPI* get_api() { return _api; }

	private:

		// Members
		tesseract::TessBaseAPI* _api = nullptr;
	};
}
