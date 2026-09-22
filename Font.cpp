#include "Font.hpp"

const uint16_t ATLAS_SIZE = 512;
// padding to assist with bleeding text
const int GLYPH_PADDING = 2;

Font::Font(std::string const &font_path, unsigned int pixel_size) {
    FT_Init_FreeType( &library );
	FT_Error error = FT_New_Face(library, font_path.c_str(), 0, &face);
	if (error)
	{
		std::cerr << "FT_New_Face failed with error code " << error << std::endl;
		return;
	}

	FT_Set_Pixel_Sizes(face, 0, pixel_size);


	hb_font = hb_ft_font_create_referenced(face);
    glGenTextures(1, &atlas);
    glBindTexture(GL_TEXTURE_2D, atlas);
    std::vector<uint8_t> blank(size_t(ATLAS_SIZE) * size_t(ATLAS_SIZE), 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, blank.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
};

Font::~Font() {
    hb_font_destroy(hb_font);
    FT_Done_Face(face); 
    FT_Done_FreeType(library);
    glDeleteTextures(1, &atlas);
}
 
GlyphInfo const &Font::get_glyph(hb_codepoint_t glyph_index) const
{
    auto iter = glyphCache.find(glyph_index); 
    if (iter != glyphCache.end())
    {
        return iter->second;
    }

    FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

    uint32_t bitmapWidth = face->glyph->bitmap.width;
    uint32_t bitmapHeight = face->glyph->bitmap.rows;

    //wrap to a new shelf before placing, so a glyph near the right edge
    //can't be given a UV rect that runs past the atlas:
    if (next_x + (int)bitmapWidth + GLYPH_PADDING > ATLAS_SIZE)
    {
        next_x = 0;
        next_y += row_height + GLYPH_PADDING;
        row_height = 0;
    }

    GlyphInfo glyph = GlyphInfo{
        (float) next_x / ATLAS_SIZE,
        (float) next_y / ATLAS_SIZE,
        (float) (next_x + bitmapWidth) / ATLAS_SIZE,
        (float) (next_y + bitmapHeight) / ATLAS_SIZE,
        bitmapWidth,
        bitmapHeight,
        face->glyph->bitmap_left,
        face->glyph->bitmap_top
    };

    glyphCache.emplace(glyph_index, glyph);

    glBindTexture(GL_TEXTURE_2D, atlas);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, next_x, next_y, (GLsizei)bitmapWidth, (GLsizei)bitmapHeight, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

    next_x += bitmapWidth + GLYPH_PADDING;
    row_height = std::max(row_height, bitmapHeight);

    return glyphCache.at(glyph_index);
};

std::vector< Vertex > Font::shape(std::string const &text) const {
    
    std::vector< Vertex > vertices; 

    hb_buffer_t *buf = hb_buffer_create();

	hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);

	hb_buffer_guess_segment_properties(buf); 

	hb_shape(hb_font, buf, nullptr, 0);

	unsigned int glyph_count; 

	hb_glyph_info_t *info_t = hb_buffer_get_glyph_infos(buf, &glyph_count); 
	hb_glyph_position_t *pos_t = hb_buffer_get_glyph_positions(buf, &glyph_count);

    float pen_x = 0.0f;
    float pen_y = 0.0f;

	for (uint32_t i = 0; i < glyph_count; i++)
	{
        uint32_t glyphIndex = info_t[i].codepoint;
        GlyphInfo const &g = get_glyph(glyphIndex); 

        //quad corners
        float left   = pen_x + (float)g.bitmap_left;
        float right  = left + (float)g.bitmap_width;
        float top    = pen_y + (float)g.bitmap_top;
        float bottom = top - (float)g.bitmap_height;

        glm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);

        Vertex top_left    { glm::vec3(left,  top,    0.0f), glm::vec2(g.uMin, g.vMin), color };
        Vertex top_right   { glm::vec3(right, top,    0.0f), glm::vec2(g.uMax, g.vMin), color };
        Vertex bottom_left { glm::vec3(left,  bottom, 0.0f), glm::vec2(g.uMin, g.vMax), color };
        Vertex bottom_right{ glm::vec3(right, bottom, 0.0f), glm::vec2(g.uMax, g.vMax), color };

        //two triangles sharing the top_right/bottom_left diagonal, since we're
        //drawing with GL_TRIANGLES (no index buffer):
        vertices.emplace_back(top_left);
        vertices.emplace_back(bottom_left);
        vertices.emplace_back(top_right);

        vertices.emplace_back(top_right);
        vertices.emplace_back(bottom_left);
        vertices.emplace_back(bottom_right);

        pen_x += pos_t[i].x_advance / 64.0f;
        pen_y += pos_t[i].y_advance / 64.0f;
	}

	hb_buffer_destroy(buf);

    return vertices;
}

float Font::measure(std::string const &text) const {
    hb_buffer_t *buf = hb_buffer_create();

	hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);

	hb_buffer_guess_segment_properties(buf);

	hb_shape(hb_font, buf, nullptr, 0);

	unsigned int glyph_count;

	hb_glyph_position_t *pos_t = hb_buffer_get_glyph_positions(buf, &glyph_count);

    float totalLen = 0.0f;

    for (uint32_t i = 0; i < glyph_count; i++)
	{
        totalLen += pos_t[i].x_advance / 64.0f;
    }

    hb_buffer_destroy(buf);

    return totalLen;
}

float Font::line_height() const {
    return face->size->metrics.height / 64.0f;
}

float Font::ascender() const {
    return face->size->metrics.ascender / 64.0f;
}