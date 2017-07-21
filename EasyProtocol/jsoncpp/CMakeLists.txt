include_directories()
add_library (jsoncpp
			 src/lib_json/json_tool.h
             src/lib_json/json_reader.cpp
			 src/lib_json/json_value.cpp
			 src/lib_json/json_writer.cpp)