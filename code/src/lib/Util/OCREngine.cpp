#include "OCREngine.hpp"

#include <Core/Core.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

namespace util
{
	OCREngine::OCREngine(OEM oem)
	{
		// Determine which mode of tesseract to use
		tesseract::OcrEngineMode mode = tesseract::OEM_LSTM_ONLY; // new stuff
		if(oem == OEM::DEPRECATED)
		{
			mode = tesseract::OEM_TESSERACT_ONLY; // old stuff, deprecated
		}
	
		// Setup tesseract TODO: what if it failes? Probably _up_api should stay nullptr and user has to check it
		const std::string tesspath = core::mt::res_path() + "/tessdata";
		_api = new tesseract::TessBaseAPI;
		_api->Init(tesspath.c_str(), "eng", mode);
		// _up_api->SetPageSegMode(tesseract::PSM_AUTO);
	}

	OCREngine::~OCREngine()
	{
		delete _api;
	}
}
