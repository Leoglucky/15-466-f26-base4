#include "PlayMode.hpp"

#include "ColorTextureProgram.hpp"
#include "ColorProgram.hpp"

#include "DrawLines.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "Font.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <sstream>

const uint32_t PIXEL_SIZE = 32;

Load< Font > fonts(LoadTagDefault, []() -> Font const * {
	Font const* font = new Font(data_path("ComicNeue-Regular.ttf"), PIXEL_SIZE);
	return font;
});

Load< Sound::Sample > sewers_music(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Sewers.opus"));
});

//All text drawn with a Font's shape() shares a vertex array object and vertex
//buffer, initialized at load time.
static GLuint text_vertex_buffer = 0;
static GLuint text_vertex_array_for_color_texture_program = 0;

static Load< void > setup_text_buffers(LoadTagDefault, [](){
	glGenBuffers(1, &text_vertex_buffer);

	glGenVertexArrays(1, &text_vertex_array_for_color_texture_program);
	glBindVertexArray(text_vertex_array_for_color_texture_program);

	glBindBuffer(GL_ARRAY_BUFFER, text_vertex_buffer);

	glVertexAttribPointer(
		color_texture_program->Position_vec4,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(Vertex),
		(GLbyte *)0 + offsetof(Vertex, position)
	);
	glEnableVertexAttribArray(color_texture_program->Position_vec4);

	glVertexAttribPointer(
		color_texture_program->Color_vec4,
		4,
		GL_FLOAT,
		GL_FALSE,
		sizeof(Vertex),
		(GLbyte *)0 + offsetof(Vertex, color)
	);
	glEnableVertexAttribArray(color_texture_program->Color_vec4);

	glVertexAttribPointer(
		color_texture_program->TexCoord_vec2,
		2,
		GL_FLOAT,
		GL_FALSE,
		sizeof(Vertex),
		(GLbyte *)0 + offsetof(Vertex, texCoords)
	);
	glEnableVertexAttribArray(color_texture_program->TexCoord_vec2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	GL_ERRORS();
});

//shapes and draws one line of text with 'font', tinted 'tint', positioned/scaled by 'object_to_clip':
static void draw_text(Font const &font, std::string const &text, glm::mat4 const &object_to_clip, glm::vec4 const &tint = glm::vec4(1.0f)) {
	std::vector< Vertex > vertices = font.shape(text);
	if (vertices.empty()) return;

	for (auto &v : vertices) v.color = tint;

	glBindBuffer(GL_ARRAY_BUFFER, text_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glUseProgram(color_texture_program->program);
	glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(object_to_clip));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, font.atlas);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(text_vertex_array_for_color_texture_program);
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glUseProgram(0);
}

//Packs whitespace-separated words from 'text' into lines no wider
//than 'max_width'
static std::vector< std::string > wrap_text(Font const &font, std::string const &text, float max_width) {
	std::vector< std::string > lines;
	std::istringstream words(text);
	std::string word;
	std::string current_line;
	while (words >> word) {
		std::string candidate = current_line.empty() ? word : current_line + " " + word;
		if (!current_line.empty() && font.measure(candidate) > max_width) {
			lines.emplace_back(current_line);
			current_line = word;
		} else {
			current_line = candidate;
		}
	}
	if (!current_line.empty()) lines.emplace_back(current_line);
	return lines;
}

//All flat-colored panels (e.g. dialogue box backgrounds) share a vertex array
//object and vertex buffer, same pattern as the text buffers above -- reuses
//DrawLines::Vertex (Position + Color) since it already matches what
//ColorProgram expects, no need for a new vertex type.
static GLuint panel_vertex_buffer = 0;
static GLuint panel_vertex_array_for_color_program = 0;

static Load< void > setup_panel_buffers(LoadTagDefault, [](){
	glGenBuffers(1, &panel_vertex_buffer);

	glGenVertexArrays(1, &panel_vertex_array_for_color_program);
	glBindVertexArray(panel_vertex_array_for_color_program);

	glBindBuffer(GL_ARRAY_BUFFER, panel_vertex_buffer);

	glVertexAttribPointer(
		color_program->Position_vec4,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(DrawLines::Vertex),
		(GLbyte *)0 + offsetof(DrawLines::Vertex, Position)
	);
	glEnableVertexAttribArray(color_program->Position_vec4);

	glVertexAttribPointer(
		color_program->Color_vec4,
		4,
		GL_UNSIGNED_BYTE,
		GL_TRUE, //normalized -- Color is a packed glm::u8vec4 here
		sizeof(DrawLines::Vertex),
		(GLbyte *)0 + offsetof(DrawLines::Vertex, Color)
	);
	glEnableVertexAttribArray(color_program->Color_vec4);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	GL_ERRORS();
});

//draws a solid-colored rectangle with corners already given in clip space:
static void draw_panel(glm::vec2 const &min_clip, glm::vec2 const &max_clip, glm::u8vec4 const &color) {
	std::vector< DrawLines::Vertex > verts;
	verts.emplace_back(glm::vec3(min_clip.x, min_clip.y, 0.0f), color);
	verts.emplace_back(glm::vec3(max_clip.x, min_clip.y, 0.0f), color);
	verts.emplace_back(glm::vec3(max_clip.x, max_clip.y, 0.0f), color);

	verts.emplace_back(glm::vec3(min_clip.x, min_clip.y, 0.0f), color);
	verts.emplace_back(glm::vec3(max_clip.x, max_clip.y, 0.0f), color);
	verts.emplace_back(glm::vec3(min_clip.x, max_clip.y, 0.0f), color);

	glBindBuffer(GL_ARRAY_BUFFER, panel_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(verts[0]), verts.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glUseProgram(color_program->program);
	glm::mat4 identity(1.0f); //verts are already in clip space
	glUniformMatrix4fv(color_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(identity));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(panel_vertex_array_for_color_program);
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(verts.size()));
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glUseProgram(0);
}

//draws one passage with options - one per line
static std::vector< PlayMode::ChoiceRegion > draw_passage(
	Font const &font, Passage const &passage,
	float left_margin_px, float bottom_margin_px, float max_width,
	glm::uvec2 const &drawable_size)
{
	std::vector< PlayMode::ChoiceRegion > regions;

	constexpr float padding = 14.0f;
	float line_height = font.line_height();

	std::vector< std::string > narrative_lines = wrap_text(font, passage.text, max_width - 2.0f * padding);

	size_t total_lines = narrative_lines.size();
	if (!passage.choices.empty()) total_lines += 1 + passage.choices.size(); //+1 for a blank spacer line
	if (total_lines == 0) return regions;

	float sx = 2.0f / float(drawable_size.x);
	float sy = 2.0f / float(drawable_size.y);
	auto pixel_to_clip = [&](float x_px, float y_px) {
		return glm::vec2(-1.0f + x_px * sx, -1.0f + y_px * sy);
	};

	float panel_left   = left_margin_px;
	float panel_right  = left_margin_px + max_width;
	float panel_bottom = bottom_margin_px;
	float panel_top    = bottom_margin_px + padding * 2.0f + line_height * float(total_lines);

	draw_panel(
		pixel_to_clip(panel_left, panel_bottom),
		pixel_to_clip(panel_right, panel_top),
		glm::u8vec4(0x00, 0x00, 0x00, 0xc0)
	);

	float baseline_y = panel_top - padding - font.ascender();

	for (auto const &narrative_line : narrative_lines) {
		glm::vec2 origin_clip = pixel_to_clip(left_margin_px + padding, baseline_y);
		glm::mat4 object_to_clip = glm::mat4(
			sx,   0.0f, 0.0f, 0.0f,
			0.0f, sy,   0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			origin_clip.x, origin_clip.y, 0.0f, 1.0f
		);
		draw_text(font, narrative_line, object_to_clip);
		baseline_y -= line_height;
	}

	if (!passage.choices.empty()) {
		baseline_y -= line_height; //blank spacer between narrative and choices

		for (auto const &choice : passage.choices) {
			//clickable band for this row, captured before baseline_y moves:
			float row_top = baseline_y + font.ascender();
			float row_bottom = baseline_y - line_height * 0.2f;

			glm::vec2 origin_clip = pixel_to_clip(left_margin_px + padding, baseline_y);
			glm::mat4 object_to_clip = glm::mat4(
				sx,   0.0f, 0.0f, 0.0f,
				0.0f, sy,   0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				origin_clip.x, origin_clip.y, 0.0f, 1.0f
			);
			//warm tint marks these lines as clickable, distinct from narrative text:
			draw_text(font, "> " + choice.label, object_to_clip, glm::vec4(1.0f, 0.85f, 0.4f, 1.0f));

			regions.push_back(PlayMode::ChoiceRegion{
				glm::vec2(panel_left, row_bottom),
				glm::vec2(panel_right, row_top),
				choice.target
			});

			baseline_y -= line_height;
		}
	}

	return regions;
}

PlayMode::PlayMode() : story(data_path("Sewer_Rats.twee")) {
	current_passage = story.start;
	music = Sound::loop(*sewers_music, 0.6f);
}

PlayMode::~PlayMode() {
	if (music) music->stop();
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN && evt.button.button == SDL_BUTTON_LEFT) {
		//SDL gives window-space coordinates (origin top-left, y down)
		//flip to match draw_passage()'s convention (origin bottom-left, y up):
		glm::vec2 click(evt.button.x, float(window_size.y) - evt.button.y);

		if (choice_regions.empty()) {
			//no choices means the current passage is an ending so click to restart:
			current_passage = story.start;
			return true;
		}

		for (auto const &region : choice_regions) {
			if (click.x >= region.min_px.x && click.x <= region.max_px.x &&
			    click.y >= region.min_px.y && click.y <= region.max_px.y) {
				current_passage = region.target;
				return true;
			}
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//nothing continuously animated right now
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	glClearColor(0.05f, 0.07f, 0.06f, 1.0f); //dark green
	glClear(GL_COLOR_BUFFER_BIT);

	Passage const &passage = story.passages.at(current_passage);

	float left_margin = 40.0f;
	float bottom_margin = 40.0f;
	float max_width = float(drawable_size.x) - 2.0f * left_margin;

	choice_regions = draw_passage(*fonts, passage, left_margin, bottom_margin, max_width, drawable_size);

	GL_ERRORS();
}
