#pragma once

#include <string>

namespace bylink::opengl {

// Parse BYGL v1 and emit a self-contained OpenGL/GLFW C++ translation unit.
std::string compileToCpp(const std::string& source);
std::string compileToCppV2(const std::string& source);

// Build a BYGL program as a native host executable using GLFW and OpenGL.
void buildExecutable(const std::string& source, const std::string& sourceName,
                     const std::string& outputPath, const std::string& cxx = "");

}  // namespace bylink::opengl
