#include <Core/Core.hpp>
#include <Util/OCREngine.hpp>
#include <cxxopts.hpp>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <experimental/filesystem>
#include <future>
#include <sstream>
#include <set>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#ifdef __linux__
namespace fs = std::experimental::filesystem;
#elif _WIN32
#include <experimental/filesystem> // C++-standard header file name
#include <filesystem> // Microsoft-specific implementation header file name
namespace fs = std::experimental::filesystem::v1;
#endif


template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 6)
{
	std::ostringstream out;
	out.precision(n);
	out << std::fixed << a_value;
	return out.str();
}

/////////////////////////////////////////////////
/// DbScan
/////////////////////////////////////////////////
// https://stackoverflow.com/questions/23842940/clustering-image-segments-in-opencv

class DbScan
{
public:
	std::map<int, int> labels;
	std::vector<cv::Rect>& data;
	int C;
	double eps;
	int mnpts;
	double* dp;
	//memoization table in case of complex dist functions
#define DP(i,j) dp[(data.size()*i)+j]
	DbScan(std::vector<cv::Rect>& _data,double _eps,int _mnpts):data(_data)
	{
		C=-1;
		for(int i=0;i<data.size();i++)
		{
			labels[i]=-99;
		}
		eps=_eps;
		mnpts=_mnpts;
	}
	void run()
	{
		dp = new double[data.size()*data.size()];
		for(int i=0;i<data.size();i++)
		{
			for(int j=0;j<data.size();j++)
			{
				if(i==j)
					DP(i,j)=0;
				else
					DP(i,j)=-1;
			}
		}
		for(int i=0;i<data.size();i++)
		{
			if(!isVisited(i))
			{
				std::vector<int> neighbours = regionQuery(i);
				if(neighbours.size()<mnpts)
				{
					labels[i]=-1;//noise
				}else
				{
					C++;
					expandCluster(i,neighbours);
				}
			}
		}
		delete [] dp;
	}
	void expandCluster(int p,std::vector<int> neighbours)
	{
		labels[p]=C;
		for(int i=0;i<neighbours.size();i++)
		{
			if(!isVisited(neighbours[i]))
			{
				labels[neighbours[i]]=C;
				std::vector<int> neighbours_p = regionQuery(neighbours[i]);
				if (neighbours_p.size() >= mnpts)
				{
					expandCluster(neighbours[i],neighbours_p);
				}
			}
		}
	}
	
	bool isVisited(int i)
	{
		return labels[i]!=-99;
	}
	
	std::vector<int> regionQuery(int p)
	{
		std::vector<int> res;
		for(int i=0;i<data.size();i++)
		{
			if(distanceFunc(p,i)<=eps)
			{
				res.push_back(i);
			}
		}
		return res;
	}
	
	double dist2d(cv::Point2d a,cv::Point2d b)
	{
		return sqrt(pow(a.x-b.x,2) + pow(a.y-b.y,2));
	}
	
	// the current distance function use the minimum distance of all 8 corners of two boxes^^^
	double distanceFunc(int ai,int bi)
	{
		if(DP(ai,bi)!=-1)
			return DP(ai,bi);
		cv::Rect a = data[ai];
		cv::Rect b = data[bi];
		/*
		Point2d cena= Point2d(a.x+a.width/2,
							  a.y+a.height/2);
		Point2d cenb = Point2d(b.x+b.width/2,
							  b.y+b.height/2);
		double dist = sqrt(pow(cena.x-cenb.x,2) + pow(cena.y-cenb.y,2));
		DP(ai,bi)=dist;
		DP(bi,ai)=dist;*/
		cv::Point2d tla =cv::Point2d(a.x,a.y);
		cv::Point2d tra =cv::Point2d(a.x+a.width,a.y);
		cv::Point2d bla =cv::Point2d(a.x,a.y+a.height);
		cv::Point2d bra =cv::Point2d(a.x+a.width,a.y+a.height);
		
		cv::Point2d tlb =cv::Point2d(b.x,b.y);
		cv::Point2d trb =cv::Point2d(b.x+b.width,b.y);
		cv::Point2d blb =cv::Point2d(b.x,b.y+b.height);
		cv::Point2d brb =cv::Point2d(b.x+b.width,b.y+b.height);
		
		double minDist = 9999999;
		
		minDist = std::min(minDist,dist2d(tla,tlb));
		minDist = std::min(minDist,dist2d(tla,trb));
		minDist = std::min(minDist,dist2d(tla,blb));
		minDist = std::min(minDist,dist2d(tla,brb));
		
		minDist = std::min(minDist,dist2d(tra,tlb));
		minDist = std::min(minDist,dist2d(tra,trb));
		minDist = std::min(minDist,dist2d(tra,blb));
		minDist = std::min(minDist,dist2d(tra,brb));
		
		minDist = std::min(minDist,dist2d(bla,tlb));
		minDist = std::min(minDist,dist2d(bla,trb));
		minDist = std::min(minDist,dist2d(bla,blb));
		minDist = std::min(minDist,dist2d(bla,brb));
		
		minDist = std::min(minDist,dist2d(bra,tlb));
		minDist = std::min(minDist,dist2d(bra,trb));
		minDist = std::min(minDist,dist2d(bra,blb));
		minDist = std::min(minDist,dist2d(bra,brb));
		DP(ai,bi)=minDist;
		DP(bi,ai)=minDist;
		return DP(ai,bi);
	}
	
	std::vector<std::vector<cv::Rect> > getGroups()
	{
		std::vector<std::vector<cv::Rect> > ret;
		for(int i=0;i<=C;i++)
		{
			ret.push_back(std::vector<cv::Rect>());
			for(int j=0;j<data.size();j++)
			{
				if(labels[j]==i)
				{
					ret[ret.size()-1].push_back(data[j]);
				}
			}
		}
		return ret;
	}
};

/////////////////////////////////////////////////
/// Main
/////////////////////////////////////////////////

// Main function
int main(int argc, const char** argv)
{
	core::mt::log_info("Welcome to the Reader of VisualStimuliDiscovery!");
	
	std::string stimulus_name = "/nih/stimuli/1_html/10"; // "/reddit/stimuli/0_html/16";

	/////////////////////////////////////////////////
	/// Command line arguments
	/////////////////////////////////////////////////

	// Variables to fill
	std::string visual_change_dataset_dir = "";
	std::string stimuli_root_dataset_dir = "";
	std::string output_dir = "";

	// Create options object
	cxxopts::Options options("VisualStimuliDiscovery Reader", "Reader software of the GazeMining project.");

	// Add options
	try
	{
		options.add_options()
			("d,visual-change-dataset", "Directory with log records.", cxxopts::value<std::string>()) // e.g., '/home/raphael/Dataset'
			("i,stimuli-root-dataset", "Directory with discovered stimuli of root layer to evaluate.", cxxopts::value<std::string>()) // e.g., "/home/stimuli/0_html"
			("o,output", "Directory for output.", cxxopts::value<std::string>()); // e.g., "/home/finder_output"
	}
	catch (cxxopts::OptionSpecException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	// Retrieve options
	try
	{
		// Parse arguments to options
		auto result = options.parse(argc, argv);

		// Visual change dataset directory
		if (result.count("visual-change-dataset"))
		{
			visual_change_dataset_dir = result["visual-change-dataset"].as<std::string>();
		}

		// Stimuli of root layer directory
		if (result.count("stimuli-root-dataset"))
		{
			stimuli_root_dataset_dir = result["stimuli-root-dataset"].as<std::string>();
		}

		// Output directory
		if (result.count("output"))
		{
			output_dir = result["output"].as<std::string>();
		}
	}
	catch (cxxopts::OptionParseException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	/////////////////////////////////////////////////
	/// Perform reading across all stimuli in dir
	/////////////////////////////////////////////////

	for (auto& s : fs::directory_iterator(stimuli_root_dataset_dir))
	{
		if (fs::is_regular_file(s) && fs::path(s).extension() == ".png")
		{
			std::string stimulus_id = fs::path(s).stem().generic_string();
			core::mt::log_info("Working on stimulus with id ", stimulus_id, " ...");

			// Open stimuli
			auto bgra_pixels = cv::imread(fs::path(s).string(), -1);

			/////////////////////////////////////////////////
			/// Perform Tesseract
			/////////////////////////////////////////////////

			// Detect text on stimuli
			cv::Mat tess_input;
			core::opencv::BGRA2Y(bgra_pixels, tess_input);
		
			// Create OCR engine
			auto sp_OCR_engine = std::make_shared<util::OCREngine>(util::OCREngine::OEM::DEPRECATED); // use traditional tesseract mode

			// Set provided image
			sp_OCR_engine->get_api()->SetImage(tess_input.data, tess_input.cols, tess_input.rows, 1, 1 * tess_input.cols);

			// List of rectangles and corresponding list of texts
			std::vector<cv::Rect> rects;
			std::vector<std::string> texts;

			// Perform optical character recognition. Check returned value. Sometimes tesseract seems to crash and returns nullptr.
			Boxa* boxes = sp_OCR_engine->get_api()->GetComponentImages(tesseract::RIL_WORD, true, NULL, NULL);
			if (boxes)
			{
				for (int i = 0; i < boxes->n; ++i) // go over boxes of found components
				{
					// TODO: In the example, the box is not deleted. But i guess this would be a good idea after cloning it...
					BOX* box = boxaGetBox(boxes, i, L_CLONE);

					// Throw away rectangle that covers entire stimulus
					if((box->x == 0 && box->y == 0 && box->w == tess_input.cols && box->h == tess_input.rows))
					{
						continue;
					}

					// Tell tesseract about the box
					sp_OCR_engine->get_api()->SetRectangle(box->x, box->y, box->w, box->h); // sets rectangle for OCR
					const char* text = sp_OCR_engine->get_api()->GetUTF8Text(); // retrieve text from box
					int conf = sp_OCR_engine->get_api()->MeanTextConf(); // retrieve confidence of OCR (between 0 and 100)

					// Check that actual text was recognized
					std::string str(text);
					if (str.length() > 3 && conf > 0 && core::misc::is_ascii(str))
					{
						rects.push_back(cv::Rect2i(box->x, box->y, box->w, box->h));
						texts.push_back(str);
					}

					// Tesseract has reserved memory for text which must be freed
					delete[] text;
				}
			}

			/*
			// Go over collected rects
			for(int i = (int) rects.size() - 1; i >= 0; i--)
			{
				// Increase size of rect
				cv::Point inflationPoint(-5, -5);
				cv::Size inflationSize(10, 10);
				rects.at(i) += inflationPoint;
				rects.at(i) += inflationSize;
			}
			*/

			/////////////////////////////////////////////////
			/// Perform Clustering with DbScan
			/////////////////////////////////////////////////

			// Perform clustering of rects
			DbScan dbscan(rects, 20, 2); // radius of a neighborhood with respect to some point and minimum number of points required to form a dense region
			dbscan.run();
			auto groups = dbscan.getGroups();

			/////////////////////////////////////////////////
			/// Read Gaze Data
			/////////////////////////////////////////////////

			// Read gaze data from file
			struct Gaze
			{
				int x = 0;
				int y = 0;
				core::long64 timestamp = 0;
			};
			std::map<std::string, std::vector<Gaze> > session_shot_gaze;
			{
				std::string gaze_filepath = stimuli_root_dataset_dir + "/" + stimulus_id + "-gaze.csv";
				std::ifstream gaze_file(gaze_filepath);

				// Load content of file into buffer
				std::stringstream buffer;
				buffer << gaze_file.rdbuf();
				gaze_file.close();

				// Go over lines
				std::string line;
				std::getline(buffer, line); // skip header
				while(std::getline(buffer, line))
				{
					if(!line.empty())
					{
						auto tokens = core::misc::tokenize(line);
						std::string key = tokens.at(0) + tokens.at(1);
						Gaze gaze;
						gaze.timestamp = std::stoll(tokens.at(2));
						gaze.x = std::stoi(tokens.at(3));
						gaze.y = std::stoi(tokens.at(4));
						session_shot_gaze[key].push_back(gaze);
					}
				}
			}

			/////////////////////////////////////////////////
			/// Draw Rects
			/////////////////////////////////////////////////

			// Draw the rects onto the stimulus
			for(const auto& group: groups) // go over groups
			{
				// Determine rect that covers entire group
				int min_x = std::numeric_limits<int>::max();
				int min_y = std::numeric_limits<int>::max();
				int max_x = 0;
				int max_y = 0;
				std::string group_text;
				for(const auto& rect : group) // go over rects in a group
				{
					// Update min and max coordinates of the group box
					min_x = rect.tl().x < min_x ? rect.tl().x : min_x;
					min_y = rect.tl().y < min_y ? rect.tl().y : min_y;
					max_x = rect.br().x > max_x ? rect.br().x : max_x;
					max_y = rect.br().y > max_y ? rect.br().y : max_y;

					// Find index rect in original vector (dumb, but works)
					for(int i = 0; i < (int)rects.size(); ++i)
					{
						if(rects.at(i) == rect)
						{
							group_text += texts.at(i) + " ";
						}
					}

					// One box per rect
					cv::rectangle(
						bgra_pixels,
						rect,
						cv::Scalar(0,255,0,255)); // bgra
				}

				// Remove space at the end of the group text
				if((int)group_text.size() > 0)
				{
					group_text.substr(0, group_text.size() - 1);
				}

				// Group rect
				cv::Point2i dilation_factor(5,5);
				cv::Point2i tl(min_x, min_y);
				cv::Point2i br(max_x, max_y);
				cv::Rect group_rect(tl - dilation_factor, br + dilation_factor);
		
				// Overlay image at group box
				cv::Mat roi = bgra_pixels(group_rect & cv::Rect(0, 0, bgra_pixels.cols, bgra_pixels.rows));
				cv::Mat color(roi.size(), CV_8UC4, cv::Scalar(255, 255, 255, 255)); 
				double alpha = 0.75;
				cv::addWeighted(color, alpha, roi, 1.0 - alpha , 0.0, roi); 

				// One box per group
				cv::rectangle(
					bgra_pixels,
					group_rect,
					cv::Scalar(0,0,255,255)); // bgra

				// Count gaze points within group rectangle
				int gaze_count = 0;
				for(const auto& r_item : session_shot_gaze)
				{
					for(const auto& r_gaze : r_item.second)
					{
						if(r_gaze.x >= tl.x && r_gaze.x <= br.x
							&& r_gaze.y >= tl.y && r_gaze.y <= br.y)
						{
							++gaze_count;
						}
					}
				}

				// Put info about group as text
				float seconds = ((float)gaze_count/90.f); // sampling frequency of eye tracker was set to 90hz
				std::string text_fix_dur = "Fix Dur: " + to_string_with_precision(seconds, 2) + "s";
				auto tokens = core::misc::tokenize(group_text, ' ');
				std::string text_word_count = "Word Count: " + std::to_string((int)tokens.size());

				// Fixation duration
				cv::putText(bgra_pixels, // target image
					text_fix_dur, // text
					tl + cv::Point(5, 15), // top-left position
					cv::FONT_HERSHEY_DUPLEX,
					0.5,
					cv::Scalar(0,0,0,255), // font color
					1);

				// Word count
				cv::putText(bgra_pixels, // target image
					text_word_count, // text
					tl + cv::Point(5, 30), // top-left position
					cv::FONT_HERSHEY_DUPLEX,
					0.5,
					cv::Scalar(0,0,0,255), // font color
					1);
			}

			cv::imwrite(output_dir + "/" + stimulus_id + ".png", bgra_pixels);
		}
	}

	core::mt::log_info("Exit application!");
	return 0;
}
