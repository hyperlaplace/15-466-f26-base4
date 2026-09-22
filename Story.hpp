#pragma once

#include <cstdint>
#include <string>
#include <vector>

//A branching-narrative script, compiled from a Twee-flavored .twee source file by
//compile-story.py (see that script for the source syntax it accepts).
//
//The engine never parses story text -- compile-story.py already resolved every
//choice's target passage down to a plain index, so playing the story back is just
//"show passages[current].text, then jump to a choice's target when picked."
struct Story {
	//throws on any parse/validation failure (e.g. file doesn't exist, or was compiled
	//by a different version of compile-story.py than Story.cpp expects):
	Story(std::string const &filename);

	struct Choice {
		std::string text;
		uint32_t target = 0; //index into 'passages'
	};
	struct Passage {
		std::string text;
		std::vector< Choice > choices; //empty means this passage ends the story
	};

	std::vector< Passage > passages;
	static constexpr uint32_t start = 0; //compile-story.py always emits the entry-point passage at index 0
};
