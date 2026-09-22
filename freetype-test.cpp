#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include "load_save_png.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

//This file exists to check that programs that use freetype / harfbuzz link properly in this base code,
//and as a scratch area for figuring out the shaping + rasterizing pipeline before it gets wired into the game.
//You probably shouldn't be looking here to learn to use either library -- these were more useful:
// freetype tutorial: https://freetype.org/freetype2/docs/tutorial/step1.html
// harfbuzz shaping example: https://harfbuzz.github.io/a-simple-shaping-example.html

//One glyph's rasterized bitmap, copied out of the FT_GlyphSlot (which gets clobbered on the next Load_Glyph call)
//together with where its top-left pixel should land relative to the whole shaped line.
struct PlacedGlyph {
	int width = 0, rows = 0;
	std::vector< uint8_t > pixels; //width*rows, row-major, top-to-bottom, one byte of coverage per pixel
	glm::ivec2 at = glm::ivec2(0); //top-left corner, in pixels, y-down, relative to the line's pen origin
};

int main(int argc, char **argv) {
	FT_Library library;
	if (FT_Init_FreeType(&library)) {
		throw std::runtime_error("FT_Init_FreeType failed.");
	}

	//NOTE: this is just a system font for local sanity-checking; the real game will ship its own licensed font.
	std::string font_path = (argc > 1 ? argv[1] : "C:/Windows/Fonts/arial.ttf");
	std::string text = (argc > 2 ? argv[2] : "Hello, world!");

	FT_Face face;
	if (FT_New_Face(library, font_path.c_str(), 0, &face)) {
		throw std::runtime_error("FT_New_Face failed to load '" + font_path + "'.");
	}
	FT_Set_Pixel_Sizes(face, 0, 48);

	//hand the FT_Face to harfbuzz so it can shape text using this exact font + size:
	hb_font_t *hb_font = hb_ft_font_create_referenced(face);

	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);
	hb_buffer_guess_segment_properties(buf); //infers script/direction/language from the buffer's text
	hb_shape(hb_font, buf, nullptr, 0);

	unsigned int glyph_count = 0;
	hb_glyph_info_t const *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
	hb_glyph_position_t const *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

	std::cout << "Shaped \"" << text << "\" into " << glyph_count << " glyphs." << std::endl;

	//walk the shaped glyphs, advancing a pen position and rasterizing + copying out each glyph's bitmap:
	std::vector< PlacedGlyph > placed;
	placed.reserve(glyph_count);

	glm::ivec2 pen = glm::ivec2(0, 0); //accumulated in whole pixels; fine for a sanity check, not for production
	glm::ivec2 min = glm::ivec2(0, 0), max = glm::ivec2(0, 0);

	for (unsigned int i = 0; i < glyph_count; ++i) {
		hb_codepoint_t glyph_index = glyph_info[i].codepoint; //NOTE: already a glyph *index* into this font, not a codepoint

		if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) {
			std::cerr << "WARNING: FT_Load_Glyph failed for glyph " << glyph_index << "; skipping." << std::endl;
			continue;
		}
		if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
			std::cerr << "WARNING: FT_Render_Glyph failed for glyph " << glyph_index << "; skipping." << std::endl;
			continue;
		}

		FT_Bitmap const &bitmap = face->glyph->bitmap;
		//harfbuzz's offsets are in a y-up, 26.6 fixed-point space; our pixel image is y-down:
		glm::ivec2 at = pen
			+ glm::ivec2(glyph_pos[i].x_offset >> 6, -(glyph_pos[i].y_offset >> 6))
			+ glm::ivec2(face->glyph->bitmap_left, -face->glyph->bitmap_top);

		if (bitmap.width > 0 && bitmap.rows > 0) {
			PlacedGlyph pg;
			pg.width = bitmap.width;
			pg.rows = bitmap.rows;
			pg.at = at;
			pg.pixels.resize(size_t(pg.width) * pg.rows);
			//copy row-by-row since bitmap.pitch may be larger than bitmap.width:
			for (int row = 0; row < pg.rows; ++row) {
				std::memcpy(&pg.pixels[size_t(row) * pg.width], bitmap.buffer + size_t(row) * bitmap.pitch, pg.width);
			}
			min = glm::min(min, at);
			max = glm::max(max, at + glm::ivec2(pg.width, pg.rows));
			placed.emplace_back(std::move(pg));
		}

		pen += glm::ivec2(glyph_pos[i].x_advance >> 6, glyph_pos[i].y_advance >> 6);
	}

	hb_buffer_destroy(buf);
	hb_font_destroy(hb_font);

	//composite all the glyph bitmaps into one image, so this is easy to eyeball:
	constexpr int margin = 4;
	glm::uvec2 size = glm::uvec2(max - min) + glm::uvec2(2 * margin);
	std::vector< glm::u8vec4 > image(size_t(size.x) * size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	for (auto const &pg : placed) {
		glm::ivec2 origin = pg.at - min + glm::ivec2(margin);
		for (int row = 0; row < pg.rows; ++row) {
			for (int col = 0; col < pg.width; ++col) {
				uint8_t coverage = pg.pixels[size_t(row) * pg.width + col];
				if (coverage == 0) continue;
				glm::ivec2 p = origin + glm::ivec2(col, row);
				if (p.x < 0 || p.y < 0 || uint32_t(p.x) >= size.x || uint32_t(p.y) >= size.y) continue;
				uint8_t shade = uint8_t(255 - coverage); //paint black text on the white background
				image[size_t(p.y) * size.x + p.x] = glm::u8vec4(shade, shade, shade, 0xff);
			}
		}
	}

	save_png("freetype-test.png", size, image.data(), UpperLeftOrigin);
	std::cout << "Wrote freetype-test.png (" << size.x << "x" << size.y << ")." << std::endl;

	FT_Done_Face(face);
	FT_Done_FreeType(library);
}
