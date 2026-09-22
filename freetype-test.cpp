#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <iostream>

//This file exists to check that programs that use freetype / harfbuzz link properly in this base code.
//You probably shouldn't be looking here to learn to use either library.

int main(int argc, char **argv) {
	FT_Library library;
	FT_Init_FreeType( &library );

	FT_Face face; 
	FT_Error error = FT_New_Face(library, "dist/ComicNeue-Regular.ttf", 0, &face);
	if (error)
	{
		std::cerr << "FT_New_Face failed with error code " << error << std::endl; 
		return 1; 
	}

	std::cout << "Loaded face: " << face->family_name << " / " << face->style_name << std::endl; 
	std::cout << " num_glyphs: " << face->num_glyphs << std::endl; 
	std::cout << " units_per_EM: " << face->units_per_EM << std::endl; 

	FT_Set_Pixel_Sizes(face, 0, 48); 

	std::string text = "Testing HarfBuzz"; 

	hb_buffer_t *buf = hb_buffer_create();
	hb_font_t *hb_font = hb_ft_font_create_referenced(face);

	hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);

	hb_buffer_guess_segment_properties(buf); 

	hb_shape(hb_font, buf, nullptr, 0);

	unsigned int glyph_count; 

	hb_glyph_info_t *info_t = hb_buffer_get_glyph_infos(buf, &glyph_count); 
	hb_glyph_position_t *pos_t = hb_buffer_get_glyph_positions(buf, &glyph_count);

	for (uint32_t i = 0; i < glyph_count; i++)
	{
		std::cout << info_t[i].codepoint << " "; 
	}
	std::cout << std::endl; 
	for (uint32_t i = 0; i < glyph_count; i++)
	{
		std::cout << pos_t[i].x_advance << " ";
		FT_Load_Glyph(face, info_t[i].codepoint, FT_LOAD_DEFAULT);
		FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
	}
	std::cout << std::endl;

	std::cout << "Bitmap Width: " << face->glyph->bitmap.width << std::endl;
	std::cout << "Bitmap Rows: " << face->glyph->bitmap.rows << std::endl; 

	hb_buffer_destroy(buf);


	std::cout << "It worked?" << std::endl;
}
