#include "Story.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

//Scratch program to sanity-check the compile-story.py -> Story.cpp round trip without
//needing the full game running: loads a compiled .story file, prints the entry passage,
//and does a breadth-first walk of the whole passage graph to make sure every passage is
//reachable and every choice's target index resolves.

int main(int argc, char **argv) {
	std::string path = (argc > 1 ? argv[1] : "dist/story.data");
	Story story(path);

	std::cout << "Loaded " << story.passages.size() << " passage(s) from '" << path << "'." << std::endl;

	Story::Passage const &start = story.passages[Story::start];
	std::cout << "\n--- start passage ---\n" << start.text << "\n";
	for (auto const &choice : start.choices) {
		std::cout << "  [" << choice.target << "] " << choice.text << std::endl;
	}

	//breadth-first walk of the whole graph, from the start passage:
	std::vector< bool > visited(story.passages.size(), false);
	std::vector< uint32_t > queue = { Story::start };
	visited[Story::start] = true;
	uint32_t endings = 0;

	std::cout << "\n--- reachability walk ---" << std::endl;
	while (!queue.empty()) {
		uint32_t at = queue.back();
		queue.pop_back();
		Story::Passage const &passage = story.passages[at];
		if (passage.choices.empty()) {
			endings += 1;
			std::cout << "passage " << at << ": ending (\"" << passage.text.substr(0, 40) << "...\")" << std::endl;
		} else {
			std::cout << "passage " << at << ": " << passage.choices.size() << " choice(s)" << std::endl;
		}
		for (auto const &choice : passage.choices) {
			if (!visited[choice.target]) {
				visited[choice.target] = true;
				queue.push_back(choice.target);
			}
		}
	}

	uint32_t unreachable = 0;
	for (uint32_t i = 0; i < visited.size(); ++i) {
		if (!visited[i]) {
			std::cerr << "WARNING: passage " << i << " is never reachable from the start passage." << std::endl;
			unreachable += 1;
		}
	}

	std::cout << "\nreached " << std::count(visited.begin(), visited.end(), true) << "/" << story.passages.size()
		<< " passage(s), found " << endings << " ending(s), " << unreachable << " unreachable." << std::endl;
}
