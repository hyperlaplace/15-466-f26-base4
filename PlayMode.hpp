#include "Mode.hpp"

#include "Story.hpp"

#include <glm/glm.hpp>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	uint32_t current = Story::start; //index of the passage currently on screen
	uint32_t selected = 0; //which of the current passage's choices is highlighted
};
