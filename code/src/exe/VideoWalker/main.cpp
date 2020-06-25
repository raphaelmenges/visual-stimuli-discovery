//! Video Walker example.
/*!
Walks over image frames of video and displays them.
*/

#include <Core/Core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <libsimplewebm.hpp>
#include <iostream>

//! Main function of Video Walker.
/*!
\return Indicator about success.
*/
int main(int argc, const char* argv[])
{
	std::string video_path = core::mt::res_path() + "test/big_buck_bunny.webm";

	std::cout << "Welcome to the Video Walker Example!" << std::endl;
	std::cout << "Path to video: " << video_path << std::endl;
	std::cout << "Press any key to start the walk." << std::endl;
	
	core::misc::wait_any_key();

#ifdef GM_VISUAL_DEBUG
	{
		// Prepares variables
		auto sp_images = std::shared_ptr<std::vector<simplewebm::Image> >(new std::vector<simplewebm::Image>); // holds retrieved images
		unsigned int extracted_image_count; // holds count of extracted images
		auto up_video_walker = simplewebm::create_video_walker(video_path); // create walker

		// Walk over frames
		int frame_number = 0;
		while (up_video_walker->walk(sp_images, 1, &extracted_image_count))
		{
			// Image to OpenCV mat
			auto p_image = &sp_images->at(0);
			cv::Mat opencv_matrix =
				cv::Mat(
					p_image->height,
					p_image->width,
					CV_8UC3,
					p_image->data.data());

			// Show image
			std::string window_title = "Frame " + std::to_string(frame_number);
			std::cout << "Displaying " << window_title << std::endl;
			cv::namedWindow(window_title, cv::WINDOW_AUTOSIZE);
			cv::imshow(window_title, opencv_matrix);
			cv::waitKey();
			cv::destroyWindow(window_title);

			// Prepare next image frame
			sp_images->clear();
			++frame_number;
		}
	}
#else
	{
		std::cout << "Error: Visual debug flag required." << std::endl;
	}
#endif

	return 0;
}