#include "PlayMode.hpp"

#include "Font.hpp"
#include "TextRenderer.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <sstream>

//NOTE: these are still system fonts, not something the game ships and owns the license for.
//TODO: swap for a properly-licensed font under assets/ before turning anything in.
Load< Font > game_font(LoadTagDefault, []() -> Font const * {
	return new Font("C:/Windows/Fonts/georgia.ttf", 32);
});
Load< Font > choice_font(LoadTagDefault, []() -> Font const * {
	return new Font("C:/Windows/Fonts/georgiab.ttf", 26);
});

Load< Story > game_story(LoadTagDefault, []() -> Story const * {
	return new Story(data_path("story.data"));
});

namespace {

//Greedily word-wraps 'text' (paragraphs already separated by '\n', as compile-story.py leaves
//them) to fit within 'max_width' pixels at 'font's rasterized size. Shaping is cheap enough at
//this scale (a passage is a couple dozen words) to just measure each candidate line directly
//with the real font instead of estimating from a fixed character width -- this is also what
//lets assets/story.twee stay plain, readable prose instead of being hand-wrapped for one
//specific window size.
std::vector< std::string > wrap_text(Font const &font, std::string const &text, float max_width) {
	std::vector< std::string > lines;

	std::istringstream paragraphs(text);
	std::string paragraph;
	while (std::getline(paragraphs, paragraph, '\n')) {
		if (paragraph.empty()) {
			lines.emplace_back(""); //preserve blank lines between paragraphs
			continue;
		}

		std::istringstream words(paragraph);
		std::string word, line;
		while (words >> word) {
			std::string candidate = line.empty() ? word : line + " " + word;
			float advance = 0.0f;
			font.layout(candidate, &advance);
			if (advance > max_width && !line.empty()) {
				lines.emplace_back(line);
				line = word;
			} else {
				line = candidate;
			}
		}
		lines.emplace_back(line);
	}

	return lines;
}

} //namespace

PlayMode::PlayMode() {
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type != SDL_EVENT_KEY_DOWN) return false;

	if (evt.key.key == SDLK_ESCAPE) {
		Mode::set_current(nullptr);
		return true;
	}

	Story::Passage const &passage = game_story->passages[current];

	if (passage.choices.empty()) {
		//this passage is an ending -- any key restarts the story:
		current = Story::start;
		selected = 0;
		return true;
	}

	uint32_t count = uint32_t(passage.choices.size());
	if (evt.key.key == SDLK_UP || evt.key.key == SDLK_W) {
		selected = (selected + count - 1) % count;
		return true;
	} else if (evt.key.key == SDLK_DOWN || evt.key.key == SDLK_S) {
		selected = (selected + 1) % count;
		return true;
	} else if (evt.key.key == SDLK_RETURN || evt.key.key == SDLK_SPACE) {
		current = passage.choices[selected].target;
		selected = 0;
		return true;
	}

	return false;
}

void PlayMode::update(float elapsed) {
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //glyph quads are opaque rects with the letterform in alpha, so this has to be on

	float aspect = float(drawable_size.x) / float(drawable_size.y);
	glm::mat4 world_to_clip = glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	float px = 2.0f / drawable_size.y; //world units per screen pixel -- keeps text exactly as many physical pixels tall as it was rasterized at, regardless of window size

	float margin = 0.12f;
	float left = -aspect + margin;
	float max_width_px = (2.0f * aspect - 2.0f * margin) / px;

	Story::Passage const &passage = game_story->passages[current];
	std::vector< std::string > lines = wrap_text(*game_font, passage.text, max_width_px);

	{ //TextRenderer's destructor does the actual GL draw, so it must be destroyed while blending is still on:
		TextRenderer text(world_to_clip);

		float y = 0.8f;
		float line_step = game_font->line_height * px;
		for (auto const &line : lines) {
			if (!line.empty()) {
				text.draw_text(*game_font, line,
					glm::vec3(left, y, 0.0f),
					glm::vec3(px, 0.0f, 0.0f), glm::vec3(0.0f, px, 0.0f),
					glm::u8vec4(0xe8, 0xe4, 0xd8, 0xff));
			}
			y -= line_step;
		}

		y -= line_step * 0.5f; //extra gap before the choices

		if (passage.choices.empty()) {
			text.draw_text(*choice_font, "-- press any key to begin again --",
				glm::vec3(left, y, 0.0f),
				glm::vec3(px, 0.0f, 0.0f), glm::vec3(0.0f, px, 0.0f),
				glm::u8vec4(0x90, 0x90, 0x90, 0xff));
		} else {
			float choice_step = choice_font->line_height * px;
			for (uint32_t i = 0; i < passage.choices.size(); ++i) {
				bool is_selected = (i == selected);
				std::string line = (is_selected ? "> " : "  ") + passage.choices[i].text;
				text.draw_text(*choice_font, line,
					glm::vec3(left, y, 0.0f),
					glm::vec3(px, 0.0f, 0.0f), glm::vec3(0.0f, px, 0.0f),
					is_selected ? glm::u8vec4(0xff, 0xd8, 0x60, 0xff) : glm::u8vec4(0xa0, 0xa0, 0xa0, 0xff));
				y -= choice_step;
			}
		}
	}

	glDisable(GL_BLEND);
	GL_ERRORS();
}
