#pragma once
#include <map>
#include <boost/utility/string_view.hpp>
#include <boost/optional.hpp>
#include <boost/any.hpp>
class Attributes {
	std::map<std::string, boost::any> attributes;
public:
	void addAttribute(boost::string_view key, boost::any value) {
		attributes.emplace(std::make_pair(key, value));
	}
	boost::optional<boost::any> getAttribute(boost::string_view key) {
		auto it = attributes.find(std::string(key));
		if (it == end(attributes)) return {};
		return it->second;
	}
	void removeAttribute(boost::string_view key) {
		auto it = attributes.find(std::string(key));
		if (it == end(attributes)) return;
		attributes.erase(it);
	}
};