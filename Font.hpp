#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <iostream>
#include <string> 
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>

#include <glm/gtc/type_ptr.hpp>
#include "gl_errors.hpp"
#include "GL.hpp"

struct GlyphInfo {
    float uMin;
    float vMin; 
    float uMax;
    float vMax; 
    uint32_t bitmap_width;
    uint32_t bitmap_height;
    int bitmap_left; 
    int bitmap_top;
};

struct Vertex {
    glm::vec3 position; 
    glm::vec2 texCoords; 
    glm::vec4 color;
};

struct Font{
    // helper functions
    // (const: rasterizing/caching a glyph doesn't change Font's logical
    // state from the outside, and Load<Font> only ever hands out const access)
    GlyphInfo const &get_glyph(hb_codepoint_t glyph_index) const;

    // entry point function
    std::vector< Vertex > shape(std::string const &text) const;

    // cache for glyphs
    // const get_glyph()/shape() calls above
    // usage: Codepoint, UV Info
    mutable std::unordered_map<hb_codepoint_t, GlyphInfo> glyphCache;

    // general declarations
    Font(std::string const &font_path, unsigned int pixel_size);
    virtual ~Font();

    float measure(std::string const &text) const;
    float line_height() const; //recommended pixel spacing between baselines, from FreeType's face metrics
    float ascender() const; //pixels from the baseline to the top of the font's tallest glyphs

    // values to be updated
    FT_Library library;
    FT_Face face;
    hb_font_t *hb_font = nullptr;

    // Bitmask map for glyphs
    GLuint atlas;

    // atlas packer satate
    mutable int next_x = 0;
    mutable int next_y = 0;
    mutable uint32_t row_height = 0;
};