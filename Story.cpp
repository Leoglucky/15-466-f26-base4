#include "Story.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

//Twee passage names can backslash-escape characters
std::string unescape(std::string const &s) {
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '\\' && i + 1 < s.size()) {
			out += s[i + 1];
			++i;
		} else {
			out += s[i];
		}
	}
	return out;
}

std::string trim(std::string const &s) {
	size_t begin = s.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos) return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(begin, end - begin + 1);
}

//parses one [[ ... ]] link body: "Display|Target", "Display->Target",
//"Target<-Display", or a bare "Target" (display text == target passage):
Choice parse_link(std::string const &body) {
	std::string s = trim(body);

	size_t arrow = s.find("->");
	if (arrow != std::string::npos) {
		return Choice{ trim(s.substr(0, arrow)), trim(s.substr(arrow + 2)) };
	}
	size_t back_arrow = s.find("<-");
	if (back_arrow != std::string::npos) {
		return Choice{ trim(s.substr(back_arrow + 2)), trim(s.substr(0, back_arrow)) };
	}
	//split on the first run of one-or-more '|' characters
	size_t bar = s.find('|');
	if (bar != std::string::npos) {
		size_t bar_end = s.find_first_not_of('|', bar);
		if (bar_end == std::string::npos) bar_end = s.size();
		return Choice{ trim(s.substr(0, bar)), trim(s.substr(bar_end)) };
	}
	//no separator
	// display text and target passage are the same:
	return Choice{ s, s };
}

//finds the value of a simple top-level "key": "value" pair in a JSON blob
std::string find_json_string(std::string const &json, std::string const &key) {
	std::string needle = "\"" + key + "\"";
	size_t at = json.find(needle);
	if (at == std::string::npos) return "";
	size_t colon = json.find(':', at + needle.size());
	if (colon == std::string::npos) return "";
	size_t quote_start = json.find('"', colon);
	if (quote_start == std::string::npos) return "";
	size_t quote_end = json.find('"', quote_start + 1);
	if (quote_end == std::string::npos) return "";
	return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

} //namespace

Story::Story(std::string const &twee_path) {
	std::ifstream file(twee_path, std::ios::binary);
	if (!file) throw std::runtime_error("Story: could not open '" + twee_path + "'");

	//split the file into lines first, then group lines under whichever
	//:: prefixed header line most recently started a new passage:
	struct RawPassage {
		std::string header;
		std::vector< std::string > body_lines;
	};
	std::vector< RawPassage > raw;

	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back(); //CRLF safety
		if (line.compare(0, 2, "::") == 0) {
			raw.push_back(RawPassage{ line, {} });
		} else if (!raw.empty()) {
			raw.back().body_lines.push_back(line);
		}
		//(lines before the first "::" header, if any, are ignored)
	}

	std::string story_data_json;

	for (auto const &rp : raw) {
		//header is "::" + name + optional "{...}" metadata on the same line;
		//this story's names never contain '{', so splitting on the first one
		//separates name from metadata:
		std::string after = rp.header.substr(2);
		size_t brace = after.find('{');
		std::string raw_name = (brace == std::string::npos) ? after : after.substr(0, brace);
		std::string name = trim(unescape(raw_name));

		if (name == "StoryTitle") continue; //not a gameplay passage
		if (name == "StoryData") {
			for (auto const &body_line : rp.body_lines) story_data_json += body_line + "\n";
			continue;
		}

		Passage passage;
		passage.name = name;

		std::string text;
		for (auto const &body_line : rp.body_lines) {
			std::string remaining = body_line;
			size_t open;
			while ((open = remaining.find("[[")) != std::string::npos) {
				text += remaining.substr(0, open);
				size_t close = remaining.find("]]", open);
				if (close == std::string::npos) { remaining.clear(); break; }
				std::string link_body = remaining.substr(open + 2, close - (open + 2));
				passage.choices.emplace_back(parse_link(link_body));
				remaining = remaining.substr(close + 2);
			}
			text += remaining;
			text += "\n";
		}
		//wrap_text() later treats all whitespace (including these embedded
		//newlines, and the ones left behind by stripped-out link lines) as
		//plain word-separators, so no further cleanup is needed here:
		passage.text = trim(text);

		passages.emplace(name, std::move(passage));
	}

	start = find_json_string(story_data_json, "start");
	if (start.empty()) throw std::runtime_error("Story: StoryData missing a 'start' passage");
	if (passages.find(start) == passages.end()) {
		throw std::runtime_error("Story: start passage '" + start + "' not found");
	}
}
