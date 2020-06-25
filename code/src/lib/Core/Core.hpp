#pragma once

#include <opencv2/core/types.hpp>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <thread>
#include <numeric>

namespace core
{
	// Typedef for long
	#ifdef __linux__
		typedef long long64;
	#elif _WIN32
		typedef long long long64;
	#endif

	// Store main thread id and add check whether one is in it (required for UI)
	extern std::thread::id main_thread_id; // declaration of global variable
	bool this_is_main_thread();

	// Thread-safe functions
	namespace mt
	{
		// Path to resources
		const std::string& res_path();

		// Logging (Problem: All executables will use the same log file)
		enum class LogLevel { info, warn, error };
		void log(LogLevel level, const std::string& message);

		template<typename... Args>
		void log_info(Args const&... args)
		{
			std::ostringstream stream; using List = int[];
			(void)List {
				0, ((void)(stream << args), 0) ...
			};
			log(LogLevel::info, stream.str());
		}

		template<typename... Args>
		void log_warn(Args const&... args)
		{
			std::ostringstream stream; using List = int[];
			(void)List {
				0, ((void)(stream << args), 0) ...
			};
			log(LogLevel::warn, stream.str());
		}

		template<typename... Args>
		void log_error(Args const&... args)
		{
			std::ostringstream stream; using List = int[];
			(void)List {
				0, ((void)(stream << args), 0) ...
			};
			log(LogLevel::error, stream.str());
		}

		// Get value from config singleton
		template<typename T>
		T get_config_value(const T& fallback, const std::vector<std::string>& path);
	}

	// Miscellaneous 
	namespace misc
	{
		// Wait for the user to press any key
		void wait_any_key();

		// Convert float to percentage print
		std::string to_percentage_str(const float& v);
		
		// Make const
		template<typename T>
		std::shared_ptr<const std::vector<std::shared_ptr<const T> > > make_const(std::shared_ptr<std::vector<std::shared_ptr<T> > > input)
		{
			auto output  = std::make_shared<std::vector<std::shared_ptr<const T> > >();
			output->reserve(input->size());
			for(const auto& r_item : *input.get())
			{
				output->push_back(r_item);
			}
			return output;
		}

		// Make set of unique strings
		std::shared_ptr<std::vector<std::string> > get_unique_strings(std::shared_ptr<const std::vector<std::string> > sp_strings);

		// Tokenize string by separator
		std::vector<std::string> tokenize(std::string str, char separator = ',');

		// Create directory if not yet existing (creates complete path if required)
		void create_directories(std::string path);
		
		// Checks whether string contains only ascii letters
		bool is_ascii(const std::string& s);
	}

	// Math
	namespace math
	{
		template<class T>
		const T& clamp(const T& x, const T& lower, const T& upper)
		{
			return std::min(upper, std::max(x, lower));
		}

		float euclidean_dist(const cv::Point2i& p, const cv::Point2i& q);
		float euclidean_dist(const cv::Point2f& p, const cv::Point2f& q);
	}

	// OpenCV
	namespace opencv
	{
		// Assume same dimension for all input matrices, alpha has only one channel, all channels have 8 bit depth
		void blend(const cv::Mat& foreground, const cv::Mat& background, const cv::Mat& alpha, cv::Mat& out);

		// Assume same dimension for all input matrices, all channels have 8 bit depth, alpha for blending taken from last channel of foreground
		void blend(const cv::Mat& foreground, const cv::Mat& background, cv::Mat& out);

		// x == horizontal, y == vertical
		// Concatenate rows and columns to fit source into target considering the given offset
		void extend(cv::Mat& target, cv::Rect roi);

		// Calculate rect that covers the non-zero alpha pixels. Return zero'd rect if no non-zero alpha pixels found
		cv::Rect covering_rect_bgra(const cv::Mat& mat); // assumes 4 channels with each 8 bit depth
		cv::Rect covering_rect_a(const cv::Mat& mat); // assumes 1 channel with 8 bit depth

		// Create matrix filled with chessboard pattern for alpha visualization. Outputs matrix of provided type
		cv::Mat create_chess_board(int width, int height, int type = CV_8UC1);

		// Take two images and return the overlapping and cropped pixels in two new images. Returns true is there has been an overlap of visible area and false if not
		bool overlap_and_crop(const cv::Mat& in1, const cv::Mat& in2, cv::Mat& out1, cv::Mat& out2);

		// Checks, whether a window that is handled by OpenCV is open
		int is_window_open(const cv::String& name);

		// Scale matrix to fit. Returns scaled copy of matrix, optionally provides scale factor via pointer in parameters.
		cv::Mat scale_to_fit(const cv::Mat& in, int max_width, int max_height, float* p_scale_factor = nullptr);

		// Translate matrix
		cv::Mat translate_matrix(cv::Mat& mat, float offset_x, float offset_y);

		// Erode alpha channel of BGRA image
		void erodeAlpha(const cv::Mat& in, cv::Mat& out, int kernel_size);

		// BGRA to Y conversion
		void BGRA2Y(const cv::Mat& in, cv::Mat& out, bool fill_transparent_with_mean = true);

		// Check for any difference in two matrices
		bool pixel_perfect_same(const cv::Mat& in1, const cv::Mat& in2);
	}

	// Functions that should be only used for testing purposes and not in production
	namespace test
	{
		// Load other config file from resources folder
		// (TODO: how to reset?, get_config_value is declared to be threadsafe.
		// However, it breaks when below function is called inbetween)
		void load_config_file(const std::string& path);
	}
}
