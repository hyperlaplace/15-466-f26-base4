#include "Story.hpp"

#include "read_write_chunk.hpp"

#include <fstream>
#include <stdexcept>

Story::Story(std::string const &filename) {
	std::ifstream file(filename, std::ios::binary);
	if (!file) {
		throw std::runtime_error("story file '" + filename + "' could not be opened.");
	}

	std::vector< char > strings;
	read_chunk(file, "str0", &strings);

	struct PassageEntry {
		uint32_t text_begin, text_end;
		uint32_t choices_begin, choices_end;
	};
	static_assert(sizeof(PassageEntry) == 16, "PassageEntry is packed.");
	std::vector< PassageEntry > passage_entries;
	read_chunk(file, "pass", &passage_entries);

	struct ChoiceEntry {
		uint32_t text_begin, text_end;
		uint32_t target;
	};
	static_assert(sizeof(ChoiceEntry) == 12, "ChoiceEntry is packed.");
	std::vector< ChoiceEntry > choice_entries;
	read_chunk(file, "chce", &choice_entries);

	//catch trailing garbage -- usually means Story.cpp and compile-story.py have drifted apart:
	{
		char extra;
		if (file.read(&extra, 1)) {
			throw std::runtime_error("story file '" + filename + "' has unexpected trailing data.");
		}
	}

	auto get_string = [&](uint32_t begin, uint32_t end) -> std::string {
		if (!(begin <= end && end <= strings.size())) {
			throw std::runtime_error("story file '" + filename + "' contains an out-of-range string reference.");
		}
		return std::string(strings.begin() + begin, strings.begin() + end);
	};

	passages.reserve(passage_entries.size());
	for (auto const &p : passage_entries) {
		Passage passage;
		passage.text = get_string(p.text_begin, p.text_end);

		if (!(p.choices_begin <= p.choices_end && p.choices_end <= choice_entries.size())) {
			throw std::runtime_error("story file '" + filename + "' contains a passage with an out-of-range choice range.");
		}
		passage.choices.reserve(p.choices_end - p.choices_begin);
		for (uint32_t c = p.choices_begin; c < p.choices_end; ++c) {
			Choice choice;
			choice.text = get_string(choice_entries[c].text_begin, choice_entries[c].text_end);
			choice.target = choice_entries[c].target;
			passage.choices.emplace_back(std::move(choice));
		}

		passages.emplace_back(std::move(passage));
	}

	if (passages.empty()) {
		throw std::runtime_error("story file '" + filename + "' contains no passages.");
	}

	for (auto const &passage : passages) {
		for (auto const &choice : passage.choices) {
			if (choice.target >= passages.size()) {
				throw std::runtime_error("story file '" + filename + "' contains a choice with an out-of-range target passage.");
			}
		}
	}
}
