#include "Core.hpp"
#include <Core/Defines.hpp>
#include <cpptoml.h>
#include <spdlog/spdlog.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <iostream>
#include <experimental/filesystem>

#ifdef __linux__ 
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#elif _WIN32
#include <conio.h>
#endif

namespace fs = std::experimental::filesystem;

/////////////////////////////////////////////////
/// get_current_path
/////////////////////////////////////////////////

#ifdef __linux__
std::string get_current_path()
{
	char pBuf[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", pBuf, PATH_MAX);
	const char *path = "";
	if (count != -1)
	{
		path = dirname(pBuf);
	}
	return path;
}
#elif _WIN32
std::string get_current_path()
{
	char pBuf[256]; size_t len = sizeof(pBuf);
	int bytes = GetModuleFileName(NULL, pBuf, (DWORD)len);
	if (bytes == 0)
	{
		return ".";
	}
	else
	{
		std::string str(pBuf);
		std::replace(str.begin(), str.end(), '\\', '/');
		str = str.substr(0, str.find_last_of("/"));
		return str;
	}
}
#endif

/////////////////////////////////////////////////
/// deployment
/////////////////////////////////////////////////

#ifdef GM_DEPLOY
static const std::string res_dir_string = get_current_path() + "/res/";
#else
static const std::string res_dir_string = std::string(GM_RES_PATH) + "/";
#endif

/////////////////////////////////////////////////
/// core
/////////////////////////////////////////////////

namespace core
{
	// Definition of global variable
	std::thread::id main_thread_id;

	// Check whether running in main thread
	bool this_is_main_thread()
	{
		return main_thread_id == std::this_thread::get_id();
	}

	// Store config as static value in this translation unit
	static std::shared_ptr<const cpptoml::table> static_sp_config = cpptoml::parse_file(mt::res_path() + "config.toml");
	
	/////////////////////////////////////////////////
	/// multi-threading
	/////////////////////////////////////////////////

	namespace mt
	{
		/////////////////////////////////////////////////
		/// res_path
		/////////////////////////////////////////////////

		const std::string& res_path()
		{
			return res_dir_string;
		}

		/////////////////////////////////////////////////
		/// log
		/////////////////////////////////////////////////

		// Singleton class
		class InternalLogger
		{
		public:

			// Constructor
			InternalLogger()
			{
				// Delegate to spdlog
				spdlog::set_async_mode(SPDLOG_ASYNC_SYNC_SIZE);
				std::vector<spdlog::sink_ptr> sinks;
				sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(get_current_path() + "/log.txt", SPDLOG_MAX_SIZE, SPDLOG_MAX_FILE_COUNT));
				sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
				_spdlogger = std::make_shared<spdlog::logger>("global_log", begin(sinks), end(sinks));
				_spdlogger->set_level(spdlog::level::info);
				_spdlogger->set_pattern("[%Y-%m-%d %T.%e, thread %t, %l] %v");
			}

			// Destructor
			~InternalLogger() { spdlog::drop_all(); }

			// Actual log method
			void log(LogLevel level, const std::string& message)
			{
				switch (level)
				{
				case LogLevel::info:
					_spdlogger->info(message);
					break;
				case LogLevel::warn:
					_spdlogger->warn(message);
					break;
				case LogLevel::error:
					_spdlogger->error(message);
					break;
				}
			}

		private:

			std::shared_ptr<spdlog::logger> _spdlogger = nullptr;
		};

		// Singleton implementation
		InternalLogger& get_internal_logger()
		{
			static InternalLogger logger;
			return logger;
		}

		// Log
		void log(LogLevel level, const std::string& message)
		{
			get_internal_logger().log(level, message);
		}

		/////////////////////////////////////////////////
		/// get_config_value
		/////////////////////////////////////////////////

		// Path within toml file for nice logging
		std::string toml_path(const std::vector<std::string>& path)
		{
			std::string result;
			const int length = static_cast<int>(path.size());
			for (int i = 0; i < length; ++i)
			{
				result += path.at(i);
				if (i < length - 1)
				{
					result += ".";
				}
			}
			return result;
		}

		// Get config value template
		template<typename T>
		T get_config_value(const T& fallback, const std::vector<std::string>& path)
		{
			// Start at root
			std::shared_ptr<const cpptoml::table> sp_table = static_sp_config;

			// Go down on path
			const int length = static_cast<int>(path.size());
			int i = 0; // start index
			for (; i < length - 1; ++i) // go through path until final key
			{
				if (sp_table)
				{
					sp_table = sp_table->get_table(path.at(i));
				}
				else
				{
					break;
				}
			}

			// Check reached key
			if (sp_table && (i == length - 1))
			{
				auto value = sp_table->get_as<T>(path.at(i));
				if (value)
				{
					return value.value_or(fallback);
				}
				else
				{
					log_warn("Config: Key or type of value not as expected: " + toml_path(path));
					return fallback;
				}
			}
			else
			{
				log_warn("Config: Path not found: " + toml_path(path));
				return fallback;
			}
		}
		
		// Pass float as double
		template<>
		float get_config_value<float>(const float& fallback, const std::vector<std::string>& path)
		{
			double d_fallback = (double)fallback;
			double result = get_config_value<double>(d_fallback, path);
			return (float)result;
		}
		
		// Explicit instantiations of above template
		template double			get_config_value<double>(const double& fallback, const std::vector<std::string>& path);
		template int			get_config_value<int>(const int& fallback, const std::vector<std::string>& path);
		template short			get_config_value<short>(const short& fallback, const std::vector<std::string>& path);
		template char			get_config_value<char>(const char& fallback, const std::vector<std::string>& path);
		template bool			get_config_value<bool>(const bool& fallback, const std::vector<std::string>& path);
		template std::string	get_config_value<std::string>(const std::string& fallback, const std::vector<std::string>& path);
	}

	/////////////////////////////////////////////////
	/// misc
	/////////////////////////////////////////////////

	namespace misc
	{
		void wait_any_key()
		{
			std::cout << "Press any key to continue." << std::endl;
#ifdef __linux__
			bool wait = true;
			while (wait)
			{
				struct termios oldt, newt;
				int ch;
				int oldf;
				tcgetattr(STDIN_FILENO, &oldt);
				newt = oldt;
				newt.c_lflag &= ~(ICANON | ECHO);
				tcsetattr(STDIN_FILENO, TCSANOW, &newt);
				oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
				fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
				ch = getchar();
				tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
				fcntl(STDIN_FILENO, F_SETFL, oldf);
				if (ch != EOF)
				{
					ungetc(ch, stdin);
					wait = false;
				}
			}
#elif _WIN32
			_getch();
#endif
		}

		std::string to_percentage_str(const float& v)
		{
			return std::to_string((int)(v * 100.f)) + "%";
		}

		std::shared_ptr<std::vector<std::string> > get_unique_strings(std::shared_ptr<const std::vector<std::string> > sp_strings)
		{
			auto sp_unique_strings = std::make_shared<std::vector<std::string> >();
			if (!sp_strings->empty())
			{
				sp_unique_strings->push_back(sp_strings->at(0));
				for (int i = 1; i < sp_strings->size(); ++i)
				{
					bool is_unique = true;
					for (int j = 0; j < sp_unique_strings->size(); ++j)
					{
						is_unique &= sp_strings->at(i) != sp_unique_strings->at(j);
						if (!is_unique)
						{
							break;
						}
					}
					if (is_unique)
					{
						sp_unique_strings->push_back(sp_strings->at(i));
					}
				}
			}
			return sp_unique_strings;
		}

		std::vector<std::string> tokenize(std::string str, char separator)
		{
			std::vector<std::string> output;
			std::vector<char> buffer;

			// Go over string
			for (int i = 0; i < (int)str.length(); i++)
			{
				// Read single character
				const char read = str.at(i);
				if (read == separator) // character is separator
				{
					if (buffer.size() > 0) // something is in buffer
					{
						output.push_back(std::string(buffer.begin(), buffer.end())); // add to output vector
						buffer.clear(); // clear buffer
					}
					else if (i > 0 && buffer.size() == 0) // insert empty string between two separators, but not directly after first one
					{
						output.push_back(""); // add empty output
					}
				}
				else // no separator
				{
					buffer.push_back(read); // just collect the character
				}
			}

			// Add remaining characters to output
			if (buffer.size() > 0)
			{
				output.push_back(std::string(buffer.begin(), buffer.end())); // add to output vector
				buffer.clear(); // clear buffer
			}

			// Return output
			return output;
		}

		void create_directories(std::string path)
		{
			if (!fs::is_directory(path) || !fs::exists(path))
			{
				fs::create_directories(path);
			}
		}
		
		bool is_ascii(const std::string& s)
		{
			return !std::any_of(s.begin(), s.end(), [](char c) // check whether any letter is not fulfilling the requirement
			{
				auto c_cast = static_cast<unsigned char>(c);
				return (c_cast > 127); // returns true for letter that is no ASCII
			});
		}

	}

	/////////////////////////////////////////////////
	/// math
	/////////////////////////////////////////////////

	namespace math
	{
		float euclidean_dist(const cv::Point2i& p, const cv::Point2i& q)
		{
			const cv::Point2i diff = p - q;
			return (float)cv::sqrt(diff.x*diff.x + diff.y*diff.y);
		}

		float euclidean_dist(const cv::Point2f& p, const cv::Point2f& q)
		{
			const cv::Point2f diff = p - q;
			return (float)cv::sqrt(diff.x*diff.x + diff.y*diff.y);
		}
	}

	/////////////////////////////////////////////////
	/// opencv
	/////////////////////////////////////////////////

	namespace opencv
	{
		void blend(const cv::Mat& foreground, const cv::Mat& background, const cv::Mat& alpha, cv::Mat& out)
		{
			assert(("blend: row count of input matrics are not equal",
				foreground.rows == background.rows
				&& background.rows == alpha.rows
				&& alpha.rows == out.rows));
			assert(("blend: column count of input matrics is not equal",
				foreground.cols == background.cols
				&& background.cols == alpha.cols
				&& alpha.cols == out.cols));
			assert(("blend: channel count of input matrics is not equal",
				foreground.channels() == background.channels()
				&& background.channels() == out.channels()
				&& alpha.channels() == 1));

			// Calculate number of pixels to process
			unsigned int pixel_count = (unsigned int)foreground.rows * (unsigned int)foreground.cols;

			// Get floating point pointers to the data matrices
			uchar* fptr = foreground.data;
			uchar* bptr = background.data;
			uchar* aptr = alpha.data;
			uchar* outptr = out.data;

			// Loop over all pixels
			for (
				unsigned int i = 0;
				i < pixel_count;
				++i, ++aptr // assume alpha has one channel
				)
			{
				float a = (*aptr); a /= 255.f;

				// Perform blending for each channel of input matrices
				for (unsigned int j = 0; j < (unsigned int)foreground.channels(); ++j, ++fptr, ++bptr, ++outptr)
				{
					float f = *fptr; f /= 255.f;
					float b = *bptr; b /= 255.f;
					*outptr = (uchar)(255 * ((f * a) + ((1.f - a) * b)));
				}
			}
		}

		void blend(const cv::Mat& foreground, const cv::Mat& background, cv::Mat& out)
		{
			assert(("blend: row count of input matrics is not equal",
				foreground.rows == background.rows
				&& background.rows == out.rows));
			assert(("blend: column count of input matrics is not equal",
				foreground.cols == background.cols
				&& background.cols == out.cols));
			assert(("blend: channel count of input matrics is not equal",
				foreground.channels() == background.channels()
				&& background.channels() == out.channels()));

			//  Calculate number of pixels to process
			unsigned int pixel_count = (unsigned int)foreground.rows * (unsigned int)foreground.cols;

			// Get floating point pointers to the data matrices
			uchar* fptr = foreground.data;
			uchar* bptr = background.data;
			uchar* outptr = out.data;
			int alpha_index = foreground.channels() - 1;

			// Loop over all pixels
			for (
				unsigned int i = 0;
				i < pixel_count;
				++i
				)
			{
				float a = (float)*(fptr + alpha_index); a /= 255.f; // TODO: alpha is then blended with alpha....

				// Perform blending for each channel of input matrices
				for (unsigned int j = 0; j < (unsigned int)foreground.channels(); ++j, ++fptr, ++bptr, ++outptr)
				{
					float f = *fptr; f /= 255.f;
					float b = *bptr; b /= 255.f;
					*outptr = (uchar)(255 * ((f * a) + ((1.f - a) * b)));
				}
			}
		}

		void extend(cv::Mat& target, cv::Rect roi)
		{
			assert(("extend: target has some dimension that size is zero",
				target.rows > 0
				&& target.cols > 0));

			// Extend target matrix if required
			int extra_rows = std::max(0, (roi.height + roi.y) - target.rows);
			int extra_cols = std::max(0, (roi.width + roi.x) - target.cols);
			if (extra_rows > 0)
			{
				cv::Mat extra_rows_mat = cv::Mat::zeros(cv::Size(target.cols, extra_rows), target.type());
				cv::vconcat(target, extra_rows_mat, target);
			}
			if (extra_cols > 0)
			{
				cv::Mat extra_cols_mat = cv::Mat::zeros(cv::Size(extra_cols, target.rows), target.type());
				cv::hconcat(target, extra_cols_mat, target);
			}
		}

		cv::Rect covering_rect_bgra(const cv::Mat& mat)
		{
			// Go over matrix and search for non-zero alpha values
			cv::Rect covered(mat.cols, mat.rows, 0, 0); // init rect with extremas
			bool found_something = false;
			for (int y = 0; y < (int)mat.rows; ++y)
			{
				for (int x = 0; x < (int)mat.cols; ++x)
				{
					if (mat.at<cv::Vec4b>(y, x)[3] > 0) // check alpha value
					{
						covered.x = covered.x > x ? x : covered.x;
						covered.y = covered.y > y ? y : covered.y;
						int pot_width = x - covered.x + 1;
						covered.width = covered.width < pot_width ? pot_width : covered.width;
						int pot_height = y - covered.y + 1;
						covered.height = covered.height < pot_height ? pot_height : covered.height;
						found_something = true;
					}
				}
			}

			// Check that at least one pixel had been set, otherwise make rect zero
			if (!found_something)
			{
				covered = cv::Rect(0, 0, 0, 0);
			}

			// Return rect
			return covered;
		}

		cv::Rect covering_rect_a(const cv::Mat& mat)
		{
			// Go over matrix and search for non-zero alpha values
			cv::Rect covered(mat.cols, mat.rows, 0, 0); // init rect with extremas
			bool found_something = false;
			for (int y = 0; y < (int)mat.rows; ++y)
			{
				for (int x = 0; x < (int)mat.cols; ++x)
				{
					if (mat.at<uchar>(y, x) > 0) // check alpha value
					{
						covered.x = covered.x > x ? x : covered.x;
						covered.y = covered.y > y ? y : covered.y;
						int pot_width = x - covered.x + 1;
						covered.width = covered.width < pot_width ? pot_width : covered.width;
						int pot_height = y - covered.y + 1;
						covered.height = covered.height < pot_height ? pot_height : covered.height;
						found_something = true;
					}
				}
			}

			// Check that at least one pixel had been set, otherwise make rect zero
			if (!found_something)
			{
				covered = cv::Rect(0, 0, 0, 0);
			}

			// Return rect
			return covered;
		}
	
		cv::Mat create_chess_board(int width, int height, int type)
		{
			assert(("create_chess_board: either width or height smaller or equal zero",
				width > 0 && height > 0));

			const int block_size = 24;
			const int double_block_size = 2 * block_size;
			cv::Mat chess_board(height, width, type, cv::Scalar::all(207));
			for (int i = 0; i < height; i = i + block_size)
			{
				for (int j = (i % double_block_size); j < width; j = j + double_block_size)
				{
					cv::Rect block(j, i, block_size, block_size);
					cv::Mat ROI = chess_board((block & cv::Rect(0, 0, width, height)));
					ROI.setTo(cv::Scalar::all(255)); // TODO: assumes type to use 8 bit per color
				}
			}
			return chess_board;
		}

		bool overlap_and_crop(const cv::Mat& in1, const cv::Mat& in2, cv::Mat& out1, cv::Mat& out2)
		{
			// Constraint ROI only to overlapping rectangular (regardless of visibility)
			cv::Rect ROI = cv::Rect(cv::Point2i(0, 0), in1.size()) & cv::Rect(cv::Point2i(0, 0), in2.size());

			// Make alpha-based mask to filter for pixels existing in both images
			std::vector<cv::Mat> bgra_planes_1;
			cv::split(in1(ROI), bgra_planes_1);
			std::vector<cv::Mat> bgra_planes_2;
			cv::split(in2(ROI), bgra_planes_2);
			cv::Mat mask = cv::min(bgra_planes_1.at(3), bgra_planes_2.at(3));

			// Compute ROI for visible area
			auto rect = core::opencv::covering_rect_a(mask);

			// Check whether there was any overlap in visible area
			if (rect.empty())
			{
				return false;
			}

			// Use mask to mask both images
			cv::Mat masked_1, masked_2;
			std::vector<cv::Mat> remerged_1 = {
				bgra_planes_1.at(0),
				bgra_planes_1.at(1),
				bgra_planes_1.at(2),
				mask // use mask instead of original alpha
			};
			std::vector<cv::Mat> remerged_2 = {
				bgra_planes_2.at(0),
				bgra_planes_2.at(1),
				bgra_planes_2.at(2),
				mask // use mask instead of original alpha
			};
			cv::merge(remerged_1, masked_1);
			cv::merge(remerged_2, masked_2);

			// Additionally, only conside content inside rectangular that is covered by mask
			out1 = masked_1(rect);
			out2 = masked_2(rect);

			return true;
		}

		// Check if an OpenCV window is open.
		// From: https://stackoverflow.com/a/48055987/29827
		int is_window_open(const cv::String& name)
		{
			return cv::getWindowProperty(name, cv::WND_PROP_AUTOSIZE) != -1;
		}

		cv::Mat scale_to_fit(const cv::Mat& in, int max_width, int max_height, float* p_scale_factor)
		{
			cv::Mat out;
			
			if(!in.empty())
			{
				// Try to fit into available space
				float scale = 1.f;
				if (in.cols > max_width)
				{
					scale = (float)max_width / (float)in.cols;
				}
				if (in.rows * scale > (float)max_height)
				{
					scale = (float)max_height / (float)in.rows;
				}
				scale = std::max(0.f, scale);
	
				// Scale matrix to fit into window
				if(scale > 0.f)
				{
					cv::resize(
						in,
						out,
						cv::Size(
						(int)(scale * (float)in.cols),
						(int)(scale * (float)in.rows)),
						0,
						0,
						cv::INTER_LINEAR);
				}
	
				// Store scale factor
				if (p_scale_factor)
				{
					*p_scale_factor = scale;
				}
			}

			return out;
		}

		cv::Mat translate_matrix(cv::Mat& mat, float offset_x, float offset_y)
		{
			cv::Mat trans_mat = (cv::Mat_<double>(2, 3) << 1, 0, offset_x, 0, 1, offset_y);
			cv::warpAffine(mat, mat, trans_mat, mat.size());
			return mat;
		}

		void erodeAlpha(const cv::Mat& in, cv::Mat& out, int kernel_size)
		{
			// Split into planes
			std::vector<cv::Mat> bgra_planes;
			cv::split(in, bgra_planes);

			// Erode alpha channel
			cv::Mat kernel = cv::getStructuringElement(
				cv::MORPH_RECT,
				cv::Size(2 * kernel_size + 1, 2 * kernel_size + 1),
				cv::Point(kernel_size, kernel_size));
			cv::erode(bgra_planes[3], bgra_planes[3], kernel, cv::Point(-1, -1), 1, 0, 0); // black border assumed

			// Merge back into BGRA image
			cv::merge(bgra_planes, out);
		}

		void BGRA2Y(const cv::Mat& in, cv::Mat& out, bool fill_transparent_with_mean)
		{
			// BGRA to BGR
			cv::cvtColor(in, out, cv::COLOR_BGRA2BGR);

			// BGR to YUV
			cv::cvtColor(out, out, cv::COLOR_BGR2YUV);

			// YUV to gray
			std::vector<cv::Mat> planes;
			cv::split(out, planes);
			out = planes[0];

			// Fill transparent values with mean
			if (fill_transparent_with_mean)
			{
				cv::Mat alpha = cv::Mat::zeros(in.size(), CV_8UC1);
				int from_to[] = { 3, 0 };
				cv::mixChannels(&in, 1, &alpha, 1, from_to, 1);
				cv::Scalar mean = cv::mean(in);
				cv::Mat back(in.rows, in.cols, CV_8UC3, cv::Scalar((int)mean[0], (int)mean[0], (int)mean[0]));
				blend(out, back, alpha, out);
			}
		}

		bool pixel_perfect_same(const cv::Mat& in1, const cv::Mat& in2)
		{
			// Go over pixels of both matrices
			bool same = true;
			for (unsigned int x = 0; x < (unsigned int)in1.cols && same; ++x)
			{
				for (unsigned int y = 0; y < (unsigned int)in1.rows && same; ++y)
				{
					// Retrieve pixel values
					const auto& pixel1 = in1.at<cv::Vec4b>(y, x);
					const auto& pixel2 = in2.at<cv::Vec4b>(y, x);

					// Check for same value
					same &= pixel1[0] == pixel2[0];
					same &= pixel1[1] == pixel2[1];
					same &= pixel1[2] == pixel2[2];
					same &= pixel1[3] == pixel2[3];
				}
			}

			return same;
		}
	}

	/////////////////////////////////////////////////
	/// test
	/////////////////////////////////////////////////

	namespace test
	{
		void load_config_file(const std::string& path)
		{
			static_sp_config = cpptoml::parse_file(mt::res_path() + path);
		}
	}
}
