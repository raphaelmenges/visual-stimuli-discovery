//! Visual debug.
/*!
Structures to track visual and structural data for debugging purposes. Only available if VISUAL_DEBUG flag has been set in CMake.
*/

#pragma once

// Macro to wrap visual debugging code
#if defined(GM_VISUAL_DEBUG)
#define VD(...) __VA_ARGS__
#else
#define VD(...)
#endif 

// All stuff required for visual debugging below
#ifdef GM_VISUAL_DEBUG

#include <opencv2/core/types.hpp>
#include <memory>
#include <vector>
#include <map>

// Class for debug-purposed dump, including visual data like OpenCV matrices. Copied from other classes and later displayed.

// Defines for simple access to create methods
#define vd_dump(name)		core::visual_debug::Dump::create(name)
#define vd_datum(name)		core::visual_debug::Datum::create(name)
#define vd_strings(name)	core::visual_debug::StringList::create(name)
#define vd_matrices(name)	core::visual_debug::MatrixList::create(name)

namespace core
{
	namespace visual_debug
	{
		class Datum;
		class Dump;

		/////////////////////////////////////////////////
		/// Values (part of datum)
		/////////////////////////////////////////////////

		// Value is one type of data to dump
		class Value
		{
		public:

			// Type is required to provide sufficient space for rendering
			enum class Type { StringList, MatrixList };

			// Constructor
			Value(Type type, std::string name = "") : _name(name), _type(type) {}

			// Destructor (to make class abstract)
			virtual ~Value() = 0;

			// Get type
			Type get_type() const { return _type; }

			// Prohibit messing up with copies
			Value & operator=(const Value&) = delete;
			Value(const Value&) = delete;

			// Friend may touch protected content
			friend class Datum;

		protected:

			// Paint value
			virtual void paint(int width, int height) const = 0;

			// Members
			std::string _name;

		private:

			// Members
			Type _type;
		};

		// Print strings
		class StringList : public Value, public std::enable_shared_from_this<StringList>
		{
		public:

			// Force the use of shared pointer
			static std::shared_ptr<StringList> create(std::string name = "")
			{
				return std::shared_ptr<StringList>(new StringList(name));
			}

			// Copies string
			std::shared_ptr<StringList> add(const std::string& value);

			// Friend may touch protected content
			friend class Datum;

		protected:

			// Paint value
			virtual void paint(int width, int height) const;

		private:

			// Constructor
			StringList(std::string name = "") : Value(Type::StringList, name) {}

			// Members
			std::vector<std::string> _strings;
		};

		class MatrixList : public Value, public std::enable_shared_from_this<MatrixList>
		{
		public:

			// Force the use of shared pointer
			static std::shared_ptr<MatrixList> create(std::string name = "")
			{
				return std::shared_ptr<MatrixList>(new MatrixList(name));
			}

			// Copies matrix. Takes optionally points to mark (e.g., keypoints of features)
			std::shared_ptr<MatrixList> add(const cv::Mat matrix, std::vector<cv::Point2i> points = {});

			// Friend may touch protected content
			friend class Datum;

		protected:

			// Paint value
			virtual void paint(int width, int height) const;

		private:

			// Constructor
			MatrixList(std::string name = "") : Value(Type::MatrixList, name) {}

			// Members
			std::vector<
				std::pair<
					std::shared_ptr<const std::vector<uchar> >, // compressed pixels
					std::vector<cv::Point2i> > > _matrices; // optional (key-) points
		};

		/////////////////////////////////////////////////
		/// Datum (part of dump or other datum)
		/////////////////////////////////////////////////

		// Datum is collection of values from one occasion (e.g., frame) to dump into one display
		class Datum : public std::enable_shared_from_this<Datum>
		{
		public:

			// Force the use of shared pointer
			static std::shared_ptr<Datum> create(std::string name = "")
			{
				return std::shared_ptr<Datum>(new Datum(name));
			}

			// Add value
			std::shared_ptr<Datum> add(std::shared_ptr<const Value> sp_value);

			// Add another datum as sub datum
			std::shared_ptr<Datum> add(std::shared_ptr<const Datum> sp_sub_datum);

			// Prohibit messing up with copies
			Datum & operator=(const Datum&) = delete;
			Datum(const Datum&) = delete;

			// Friend may touch protected content
			friend class Dump;

		private:

			// Paint datum
			void paint(int datum_depth = 0) const;

			// Constructor
			Datum(std::string name = "") : _name(name) {}

			// Members
			std::string _name;
			std::vector<std::shared_ptr<const Value> > _values;
			std::vector<std::shared_ptr<const Datum> > _sub_dates;

			// Members for painting
			mutable bool _show_sub_dates = false;
			mutable int _sub_datum_idx = 0;
		};

		/////////////////////////////////////////////////
		/// Dump
		/////////////////////////////////////////////////

		// Dump is a collection of dates that can be navigated
		class Dump : public std::enable_shared_from_this<Dump>
		{
		public:

			// Force the use of shared pointer
			static std::shared_ptr<Dump> create(std::string name = "")
			{
				return std::shared_ptr<Dump>(new Dump(name));
			}

			// Add datum
			std::shared_ptr<Dump> add(std::shared_ptr<Datum> sp_datum);

			// Display should be only called in main thread. Program pauses when dump is displayed and handled
			void display() const;
			
			// Get name
			std::string get_name() const { return _name; }

			// Prohibit messing up with copies
			Dump & operator=(const Dump&) = delete;
			Dump(const Dump&) = delete;

			// Friend may touch protected content
			friend class Explorer;

		private:
		
			// Paint dump. Might be call by this or explorer. Returns true if user indicates to exit dump
			bool paint(cv::Mat frame) const;

			// Constructor
			Dump(std::string name = "") : _name(name) {}

			// Members
			std::string _name;
			std::vector<std::shared_ptr<const Datum> > _dates;
			
			// Members for painting
			mutable int _datum_idx = 0;
		};
		

		/////////////////////////////////////////////////
		/// Explorer
		/////////////////////////////////////////////////

		// Explorer is a collection of dumps (optional, dump can be also used on its own)
		class Explorer
		{
		public:
		
			// Constructor
			Explorer(std::string name = "") {}

			// Create dump
			std::shared_ptr<Dump> create_dump(std::string name = "", std::string category = "general");

			// Display should be only called in main thread. Program pauses when explorer is displayed and handled
			void display() const;

			// Prohibit messing up with copies
			Explorer & operator=(const Explorer&) = delete;
			Explorer(const Explorer&) = delete;

		private:

			// Members
			std::map<std::string, std::vector<std::shared_ptr<const Dump> > > _dumps_map; // category mapped to dumps
		};
	}
}

#endif // GM_VISUAL_DEBUG
