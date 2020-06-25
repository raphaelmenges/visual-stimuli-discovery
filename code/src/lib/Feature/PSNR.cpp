#include "PSNR.hpp"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

namespace feature
{
	PSNR::PSNR(
		std::shared_ptr<const cv::Mat> a,
		std::shared_ptr<const cv::Mat> b)
		:
		Interface(a, b)
	{
		// Taken from: https://docs.opencv.org/2.4/doc/tutorials/highgui/video-input-psnr-ssim/video-input-psnr-ssim.html

		Mat s1;
		absdiff(*a, *b, s1);       // |I1 - I2|
		s1.convertTo(s1, CV_32F);  // cannot make a square on 8 bits
		s1 = s1.mul(s1);           // |I1 - I2|^2
		
		Scalar s = sum(s1);         // sum elements per channel
		
		double sse = s.val[0] + s.val[1] + s.val[2]; // sum channels
		
		double psnr = 0.0;
		if( sse > 1e-10) // for small values return zero
		{
			double mse = sse /(double)(a->channels() * a->total());
			psnr = 10.0*log10((255*255)/mse);
		}

		// Store feature of peak signal-to-noise ratio
		_features["psnr"] = psnr;
	}
}
