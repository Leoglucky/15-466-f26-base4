#pragma once

//Loads a Twee 3 file (Twine's plain-text export format) into a graph of
//passages the game can walk at runtime. Not a general Twee/Harlowe parser --
//just enough of it to read this game's story file: passage headers
//("Name {...json metadata...}"), [[links]] (with "Display|Target",
//"Display->Target", or bare "Target" forms), and StoryData's "start" field.

#include <string>
#include <unordered_map>
#include <vector>

struct Choice {
	std::string label;  //text shown to the player
	std::string target; //name of the passage this choice leads to
};

struct Passage {
	std::string name;
	std::string text; //narrative text, with [[links]] already stripped out
	std::vector< Choice > choices;
};

struct Story {
	Story(std::string const &twee_path);

	std::unordered_map< std::string, Passage > passages;
	std::string start; //name of the first passage to show
};
