#include "harness.h"
#include "opengl/bygl.h"
#include "common/diagnostic.h"
#include <fstream>
#include <iterator>

using bylink::CompileError;

TEST(bygl_emits_native_opengl_program) {
  const std::string source =
      "bygl 1\n"
      "window 800 600 \"BYlink Demo\"\n"
      "clear 0.02 0.03 0.05\n"
      "camera 4 3 6 0 0 0\n"
      "grid 10 1.0 0.25 0.30 0.38\n"
      "cube 0 0.5 0 1.0 0.2 0.5 1.0\n"
      "sphere 2 0.5 0 0.5 1 0.4 0.1\n"
      "rotate 0 20 0\n"
      "text 16 16 \"wollowrodk\" 1 1 1\n"
      "run\n";
  std::string cpp = bylink::opengl::compileToCpp(source);
#ifdef _WIN32
  CHECK(cpp.find("#include <windows.h>") != std::string::npos);
  CHECK(cpp.find("wglCreateContext") != std::string::npos);
  CHECK(cpp.find("CreateWindowA") != std::string::npos);
#else
  CHECK(cpp.find("#include <GLFW/glfw3.h>") != std::string::npos);
  CHECK(cpp.find("glfwCreateWindow(800,600,\"BYlink Demo\"") != std::string::npos);
#endif
  CHECK(cpp.find("glBegin(GL_QUADS)") != std::string::npos);
  CHECK(cpp.find("sphere(2.0f,0.5f,0.0f,0.5f,1.0f,0.4f,0.1f)") != std::string::npos);
  CHECK(cpp.find("text2(16.0f,16.0f,\"wollowrodk\"") != std::string::npos);
}

TEST(bygl_accepts_2d_overlay_primitives) {
  const std::string source =
      "bygl 1\nwindow 640 480 \"Overlay\"\n"
      "rect 10 20 100 40 1 0 0\n"
      "circle 180 60 24 0 1 0\n"
      "text 20 100 \"HELLO 42\" 1 1 0\nrun\n";
  std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("rect2(10.0f,20.0f,100.0f,40.0f,1.0f,0.0f,0.0f)") != std::string::npos);
  CHECK(cpp.find("circle2(180.0f,60.0f,24.0f,0.0f,1.0f,0.0f)") != std::string::npos);
  CHECK(cpp.find("text2(20.0f,100.0f,\"HELLO 42\"") != std::string::npos);
}

TEST(bygl_rejects_unknown_instruction_with_line) {
  try {
    (void)bylink::opengl::compileToCpp("bygl 1\nwindow 640 480 \"x\"\nfrobnicate 1\nrun\n");
    CHECK(false);
  } catch (const CompileError& e) {
    CHECK(e.category() == bylink::Category::Syntax);
    CHECK(e.loc().line == 3u);
  }
}

TEST(bygl_requires_run_and_accepts_unquoted_text) {
  const std::string cpp = bylink::opengl::compileToCpp(
      "bygl 1\nwindow 640 480 Overlay Window\ntext 1 2 HELLO WORLD 1 1 1\nrun\n");
  CHECK(cpp.find("text2(1.0f,2.0f,\"HELLO WORLD\"") != std::string::npos);
  try {
    (void)bylink::opengl::compileToCpp("bygl 1\nwindow 640 480 \"x\"\n");
    CHECK(false);
  } catch (const CompileError& e) {
    CHECK(e.category() == bylink::Category::Syntax);
  }
}
TEST(bygl_accepts_pong_3d_title_with_spaces) {
  const std::string source =
      "bygl 1\n"
      "window 1280 720 \"PONG 3D\"\n"
      "clear 0.02 0.02 0.08\n"
      "camera 0 8 12 0 0 0\n"
      "grid 14 1 0.15 0.35 0.15\n"
      "cube -6.5 0.4 0 0.4 0.2 0.6 1\n"
      "sphere 0.5 0.5 0 0.3 1 0.8 0.2\n"
      "text 20 20 \"PONG 3D\" 0.6 0.9 1\n"
      "text 560 30 \"0 : 0\" 1 1 1\n"
      "run\n";
  const std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("\"PONG 3D\"") != std::string::npos);
  CHECK(cpp.find("text2(20.0f,20.0f,\"PONG 3D\"") != std::string::npos);
  CHECK(cpp.find("text2(560.0f,30.0f,\"0 : 0\"") != std::string::npos);
}


TEST(bygl_accepts_smart_quotes_and_multiword_text) {
  const std::string open = "\xE2\x80\x9C";
  const std::string close = "\xE2\x80\x9D";
  const std::string source =
      std::string("bygl 1\nwindow 1280 720 ") + open + "PONG 3D" + close +
      "\nclear 0.02 0.02 0.08\ntext 20 20 " + open + "PONG 3D" + close +
      " 0.6 0.9 1\ntext 560 30 " + open + "0 : 0" + close + " 1 1 1\nrun\n";
  const std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("PONG 3D") != std::string::npos);
  CHECK(cpp.find("text2(20.0f,20.0f,\"PONG 3D\"") != std::string::npos);
  CHECK(cpp.find("text2(560.0f,30.0f,\"0 : 0\"") != std::string::npos);
}

TEST(bygl_compiles_the_complete_user_pong_scene) {
  const std::string source =
      "bygl 1\n"
      "window 1280 720 \"PONG 3D\"\n"
      "clear 0.02 0.02 0.08\n"
      "camera 0 8 12 0 0 0\n"
      "grid 14 1 0.15 0.35 0.15\n"
      "line3 -7 0 -4 7 0 -4 0.9 0.9 0.9\n"
      "line3 7 0 -4 7 0 4 0.9 0.9 0.9\n"
      "line3 7 0 4 -7 0 4 0.9 0.9 0.9\n"
      "line3 -7 0 4 -7 0 -4 0.9 0.9 0.9\n"
      "line3 0 0 -4 0 1 -4 0.8 0.8 0.8\n"
      "line3 0 0 -3 0 1 -3 0.8 0.8 0.8\n"
      "line3 0 0 -2 0 1 -2 0.8 0.8 0.8\n"
      "line3 0 0 -1 0 1 -1 0.8 0.8 0.8\n"
      "line3 0 0 0 0 1 0 0.8 0.8 0.8\n"
      "line3 0 0 1 0 1 1 0.8 0.8 0.8\n"
      "line3 0 0 2 0 1 2 0.8 0.8 0.8\n"
      "line3 0 0 3 0 1 3 0.8 0.8 0.8\n"
      "line3 0 0 4 0 1 4 0.8 0.8 0.8\n"
      "line3 0 1 -4 0 1 4 0.8 0.8 0.8\n"
      "cube -6.5 0.4 0 0.4 0.2 0.6 1\n"
      "cube -6.5 0.8 0 0.4 0.2 0.6 1\n"
      "cube -6.5 1.2 0 0.4 0.2 0.6 1\n"
      "cube 6.5 0.4 0 0.4 1 0.4 0.3\n"
      "cube 6.5 0.8 0 0.4 1 0.4 0.3\n"
      "cube 6.5 1.2 0 0.4 1 0.4 0.3\n"
      "sphere 0.5 0.5 0 0.3 1 0.8 0.2\n"
      "rotate 0 0 0\n"
      "text 20 20 \"PONG 3D\" 0.6 0.9 1\n"
      "text 560 30 \"0 : 0\" 1 1 1\n"
      "run\n";
  const std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("1280,720") != std::string::npos);
  CHECK(cpp.find("text2(20.0f,20.0f,\"PONG 3D\"") != std::string::npos);
  CHECK(cpp.find("text2(560.0f,30.0f,\"0 : 0\"") != std::string::npos);
  CHECK(cpp.find("glBegin(GL_LINES)") != std::string::npos);
  CHECK(cpp.find("glBegin(GL_QUADS)") != std::string::npos);
}


TEST(bygl2_compiles_interactive_state_input_and_collision_logic) {
  const std::string source =
      "bygl 2\nwindow 800 600 \"Game\"\nclear 0 0 0\n"
      "var x 0\nvar vx 2\nupdate\n"
      "if key W\nadd x mul -4 dt\nelse\nadd x mul 4 dt\nend\n"
      "if gt x 5\nset x 5\nset vx neg abs vx\nend\n"
      "draw\nbox x 0 0 1 2 1 1 0 0\nsphere x 1 0 0.3 1 1 0\n"
      "label 20 20 x 1 1 1\nrun\n";
  const std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("void update(State& s,double dt,double time)") != std::string::npos);
  CHECK(cpp.find("keyDown(87)") != std::string::npos);
  CHECK(cpp.find("s.x+=") != std::string::npos);
  CHECK(cpp.find("if((((s.x)>(5.0)))!=0.0)") != std::string::npos);
  CHECK(cpp.find("boxDraw(static_cast<float>(s.x)") != std::string::npos);
  CHECK(cpp.find("numberText(s.x)") != std::string::npos);
  CHECK(cpp.find("while(running)") != std::string::npos || cpp.find("while(!glfwWindowShouldClose") != std::string::npos);
}

TEST(bygl2_rejects_unknown_variables_and_unclosed_if) {
  try {
    (void)bylink::opengl::compileToCpp("bygl 2\nwindow 640 480 Game\nvar x 0\nupdate\nset x add x missing\ndraw\nrun\n");
    CHECK(false);
  } catch (const CompileError& e) { CHECK(e.category() == bylink::Category::Syntax); }
  try {
    (void)bylink::opengl::compileToCpp("bygl 2\nwindow 640 480 Game\nvar x 0\nupdate\nif gt x 0\nadd x 1\ndraw\nrun\n");
    CHECK(false);
  } catch (const CompileError& e) { CHECK(e.category() == bylink::Category::Syntax); }
}

TEST(bygl2_compiles_pong_game_example) {
  std::ifstream in(std::string(BYLINK_EXAMPLES_DIR) + "/pong.bygl", std::ios::binary);
  CHECK(in.good());
  std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("PONG 3D") != std::string::npos);
  CHECK(cpp.find("keyDown(87)") != std::string::npos);
  CHECK(cpp.find("keyDown(1001)") != std::string::npos);
  CHECK(cpp.find("s.leftScore+=1.0") != std::string::npos);
  CHECK(cpp.find("sphereDraw(static_cast<float>(s.bx)") != std::string::npos);
}


TEST(bygl2_accepts_smart_quoted_window_title) {
  const std::string open = "\xE2\x80\x9C";
  const std::string close = "\xE2\x80\x9D";
  const std::string source =
      std::string("bygl 2\nwindow 800 600 ") + open + "PONG 3D FRAME" + close +
      "\nvar x 0\nupdate\nadd x dt\ndraw\ntext 10 10 " + open + "SCORE" + close +
      " 1 1 1\nlabel 20 20 x 1 1 1\nrun\n";
  const std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("PONG 3D FRAME") != std::string::npos);
  CHECK(cpp.find("SCORE") != std::string::npos);
}

TEST(bygl2_circle_accepts_rgb_and_optional_rgba) {
  const std::string source =
      "bygl 2\nwindow 640 480 \"Circle alpha\"\n"
      "update\ndraw\n"
      "circle 100 100 20 1 0 0\n"
      "circle 200 100 20 0 1 0 0.5\n"
      "run\n";
  const std::string cpp = bylink::opengl::compileToCpp(source);
  CHECK(cpp.find("circleDraw(static_cast<float>(100.0),static_cast<float>(100.0),static_cast<float>(20.0),static_cast<float>(1.0),static_cast<float>(0.0),static_cast<float>(0.0),1.0f)") != std::string::npos);
  CHECK(cpp.find("circleDraw(static_cast<float>(200.0),static_cast<float>(100.0),static_cast<float>(20.0),static_cast<float>(0.0),static_cast<float>(1.0),static_cast<float>(0.0),static_cast<float>(0.5))") != std::string::npos);
}
