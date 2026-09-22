#include "Mode.hpp"

#include "Sound.hpp"
#include "Story.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//the story graph (parsed once from a .twee file) and which passage we're on:
	Story story;
	std::string current_passage;

	//clickable regions for the current passage's choices -- recomputed every
	//draw() call, and used by handle_event() to figure out what a click hit.
	//(pixel space, origin at the screen's bottom-left, y increasing upward --
	//same convention draw_passage()/draw_text_box() use internally)
	struct ChoiceRegion {
		glm::vec2 min_px, max_px;
		std::string target;
	};
	std::vector< ChoiceRegion > choice_regions;

	//looping background music:
	std::shared_ptr< Sound::PlayingSample > music;
};
