//! Score result class for models.
/*!
Provides score between zero and one (as float). Might be used in different scenarios with different meanings.
*/

#pragma once

#include <Core/Core.hpp>

namespace util
{
	// Score
	template <bool CLAMP = true>
	class Score
	{
	public:

		// Constructors
		Score(float value = 0.f)
		{
			set(value);
		}

		// Getter. Returns NaN if not set
		float value() const { return _value; }

		// Setter
		void set(float value)
		{
			if (CLAMP)
			{
				value = core::math::clamp(value, 0.f, 1.f);
			}
			_value = value;
		}

		// Add
		void add(float value)
		{
			set(value + _value);
		}

	private:

		// Members
		float _value = std::numeric_limits<float>::quiet_NaN(); // actual value
	};

	// Accumulation of scores
	template <bool CLAMP = true>
	class ScoreAcc
	{
	public:

		// Constructors
		ScoreAcc() {}
		ScoreAcc(Score<CLAMP> score) : ScoreAcc({ score }) {}
		ScoreAcc(std::vector<Score<CLAMP> > scores) : _scores(scores) {}

		// Push back score
		void push_back(Score<CLAMP> score) { _scores.push_back(score); }

		// Calculate average of scores. Returns zero is no score had been added
		float calc_average() const
		{
			float acc = 0.f;
			const int size = (int)_scores.size();
			if (size > 0)
			{
				std::for_each(_scores.begin(), _scores.end(), [&](const Score<CLAMP>& s) { acc += s.value(); });
				acc /= size;
			}
			return acc;
		}

	private:

		// Members
		std::vector<Score<CLAMP> > _scores;
	};
}