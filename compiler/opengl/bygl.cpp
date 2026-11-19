#include "opengl/bygl.h"
#include "opengl/bygl2.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "common/diagnostic.h"
#include "common/process.h"

namespace bylink::opengl {
namespace {

enum class Kind { Grid, Cube, Sphere, Line3, Rotate, Rect, Circle, Text };

struct Command {
  Kind kind{};
  std::vector<double> values;
  std::string text;
  uint32_t line = 0;
};

struct Program {
  int width = 960;
  int height = 640;
  std::string title = "BYlink OpenGL";
  std::array<double, 3> clear{0.035, 0.045, 0.075};
  std::array<double, 6> camera{4.0, 3.0, 6.0, 0.0, 0.0, 0.0};
  std::array<double, 3> rotation{0.0, 0.0, 0.0};
  bool hasWindow = false;
  bool hasCamera = false;
  bool hasRotation = false;
  std::vector<Command> commands;
};

struct Token {
  std::string value;
  bool quoted = false;
};

[[noreturn]] void syntax(uint32_t line, const std::string& message) {
  throw CompileError(Category::Syntax, Loc{line, 1}, "BYGL: " + message);
}

std::vector<Token> tokenize(const std::string& line, uint32_t lineNumber) {
  std::vector<Token> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    if (i >= line.size() || line[i] == '#') break;
    if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') break;
    if (line[i] == '"') {
      ++i;
      std::string value;
      bool closed = false;
      while (i < line.size()) {
        char c = line[i++];
        if (c == '"') { closed = true; break; }
        if (c == '\\') {
          if (i >= line.size()) syntax(lineNumber, "unfinished escape in string");
          char e = line[i++];
          switch (e) {
            case 'n': value.push_back('\n'); break;
            case 't': value.push_back('\t'); break;
            case 'r': value.push_back('\r'); break;
            case '\\': value.push_back('\\'); break;
            case '"': value.push_back('"'); break;
            default: syntax(lineNumber, std::string("unsupported string escape \\") + e + "'");
          }
        } else {
          value.push_back(c);
        }
      }
      if (!closed) syntax(lineNumber, "unterminated quoted string");
      out.push_back({std::move(value), true});
      if (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
        syntax(lineNumber, "expected whitespace after quoted string");
    } else {
      size_t start = i;
      while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
      out.push_back({line.substr(start, i - start), false});
    }
  }
  return out;
}

double number(const Token& token, uint32_t line) {
  if (token.quoted) syntax(line, "expected a number, found a string");
  size_t used = 0;
  double value = 0.0;
  try { value = std::stod(token.value, &used); }
  catch (...) { syntax(line, "invalid number '" + token.value + "'"); }
  if (used != token.value.size() || !std::isfinite(value))
    syntax(line, "invalid finite number '" + token.value + "'");
  return value;
}

int integer(const Token& token, uint32_t line, const char* what) {
  double value = number(token, line);
  if (std::floor(value) != value || value < 0 || value > 100000)
    syntax(line, std::string(what) + " must be an integer between 0 and 100000");
  return static_cast<int>(value);
}

void expectCount(const std::vector<Token>& t, size_t count, uint32_t line, const std::string& command) {
  if (t.size() != count + 1) {
    syntax(line, "'" + command + "' expects " + std::to_string(count) + " argument(s), got " +
                 std::to_string(t.size() - 1));
  }
}

std::string stripOuterQuotes(std::string value) {
  const std::pair<std::string, std::string> pairs[] = {
      {"\"", "\""}, {"'", "'"},
      {"\xE2\x80\x9C", "\xE2\x80\x9D"}, // “...”
      {"\xE2\x80\x98", "\xE2\x80\x99"}, // ‘...’
  };
  for (const auto& pair : pairs) {
    if (value.size() >= pair.first.size() + pair.second.size() &&
        value.compare(0, pair.first.size(), pair.first) == 0 &&
        value.compare(value.size() - pair.second.size(), pair.second.size(), pair.second) == 0) {
      value = value.substr(pair.first.size(), value.size() - pair.first.size() - pair.second.size());
      break;
    }
  }
  return value;
}

std::string joinTextTokens(const std::vector<Token>& tokens, size_t first, size_t last) {
  std::string value;
  for (size_t i = first; i < last; ++i) {
    if (i != first) value.push_back(' ');
    value += tokens[i].value;
  }
  return stripOuterQuotes(std::move(value));
}

void validateColor(const std::vector<double>& v, size_t offset, uint32_t line) {
  for (size_t i = offset; i < offset + 3; ++i) {
    if (v[i] < 0.0 || v[i] > 1.0) syntax(line, "color components must be between 0 and 1");
  }
}

Program parse(const std::string& source) {
  Program p;
  std::istringstream input(source);
  std::string line;
  uint32_t lineNumber = 0;
  bool header = false;
  bool run = false;
  size_t commandCount = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    auto t = tokenize(line, lineNumber);
    if (t.empty()) continue;
    if (!header) {
      if (t.size() != 2 || t[0].value != "bygl" || t[1].value != "1")
        syntax(lineNumber, "expected the header 'bygl 1'");
      header = true;
      continue;
    }
    if (run) syntax(lineNumber, "no instructions are allowed after 'run'");
    const std::string& op = t[0].value;
    if (++commandCount > 10000) syntax(lineNumber, "program exceeds the 10000-instruction limit");
    if (op == "window") {
      // The title is the complete remainder of the line. This accepts normal
      // ASCII quotes, smart quotes copied from rich-text editors, and bare
      // multi-word titles generated by models that alter quotation marks.
      if (t.size() < 4) syntax(lineNumber, "'window' expects width, height and a title");
      if (p.hasWindow) syntax(lineNumber, "'window' may only appear once");
      p.width = integer(t[1], lineNumber, "window width");
      p.height = integer(t[2], lineNumber, "window height");
      if (p.width < 64 || p.width > 8192 || p.height < 64 || p.height > 8192)
        syntax(lineNumber, "window dimensions must be between 64 and 8192 pixels");
      p.title = joinTextTokens(t, 3, t.size());
      if (p.title.empty()) syntax(lineNumber, "window title cannot be empty");
      p.hasWindow = true;
    } else if (op == "clear") {
      expectCount(t, 3, lineNumber, op);
      for (size_t i = 0; i < 3; ++i) p.clear[i] = number(t[i + 1], lineNumber);
      std::vector<double> c(p.clear.begin(), p.clear.end());
      validateColor(c, 0, lineNumber);
    } else if (op == "camera") {
      expectCount(t, 6, lineNumber, op);
      if (p.hasCamera) syntax(lineNumber, "'camera' may only appear once");
      for (size_t i = 0; i < 6; ++i) p.camera[i] = number(t[i + 1], lineNumber);
      p.hasCamera = true;
    } else if (op == "rotate") {
      expectCount(t, 3, lineNumber, op);
      if (p.hasRotation) syntax(lineNumber, "'rotate' may only appear once");
      for (size_t i = 0; i < 3; ++i) p.rotation[i] = number(t[i + 1], lineNumber);
      p.hasRotation = true;
    } else if (op == "grid") {
      expectCount(t, 5, lineNumber, op);
      Command c{Kind::Grid, {}, {}, lineNumber};
      c.values.push_back(integer(t[1], lineNumber, "grid divisions"));
      c.values.push_back(number(t[2], lineNumber));
      for (size_t i = 3; i < 6; ++i) c.values.push_back(number(t[i], lineNumber));
      if (c.values[0] < 1 || c.values[0] > 200 || c.values[1] <= 0.0)
        syntax(lineNumber, "grid divisions must be 1..200 and spacing must be positive");
      validateColor(c.values, 2, lineNumber);
      p.commands.push_back(std::move(c));
    } else if (op == "cube") {
      expectCount(t, 7, lineNumber, op);
      Command c{Kind::Cube, {}, {}, lineNumber};
      for (size_t i = 1; i < 8; ++i) c.values.push_back(number(t[i], lineNumber));
      if (c.values[3] <= 0.0) syntax(lineNumber, "cube size must be positive");
      validateColor(c.values, 4, lineNumber);
      p.commands.push_back(std::move(c));
    } else if (op == "sphere") {
      expectCount(t, 7, lineNumber, op);
      Command c{Kind::Sphere, {}, {}, lineNumber};
      for (size_t i = 1; i < 8; ++i) c.values.push_back(number(t[i], lineNumber));
      if (c.values[3] <= 0.0) syntax(lineNumber, "sphere radius must be positive");
      validateColor(c.values, 4, lineNumber);
      p.commands.push_back(std::move(c));
    } else if (op == "line3") {
      expectCount(t, 9, lineNumber, op);
      Command c{Kind::Line3, {}, {}, lineNumber};
      for (size_t i = 1; i < 10; ++i) c.values.push_back(number(t[i], lineNumber));
      validateColor(c.values, 6, lineNumber);
      p.commands.push_back(std::move(c));
    } else if (op == "rect") {
      expectCount(t, 7, lineNumber, op);
      Command c{Kind::Rect, {}, {}, lineNumber};
      for (size_t i = 1; i < 8; ++i) c.values.push_back(number(t[i], lineNumber));
      if (c.values[2] < 0.0 || c.values[3] < 0.0) syntax(lineNumber, "rectangle width and height cannot be negative");
      validateColor(c.values, 4, lineNumber);
      p.commands.push_back(std::move(c));
    } else if (op == "circle") {
      expectCount(t, 6, lineNumber, op);
      Command c{Kind::Circle, {}, {}, lineNumber};
      for (size_t i = 1; i < 7; ++i) c.values.push_back(number(t[i], lineNumber));
      if (c.values[2] < 0.0) syntax(lineNumber, "circle radius cannot be negative");
      validateColor(c.values, 3, lineNumber);
      p.commands.push_back(std::move(c));
    } else if (op == "text") {
      // Last three operands are RGB; everything between x/y and RGB is text.
      // Joining the middle tokens makes text robust to smart-quote conversion.
      if (t.size() < 7) syntax(lineNumber, "'text' expects x, y, text and RGB color");
      const size_t red = t.size() - 3;
      Command c{Kind::Text, {}, joinTextTokens(t, 3, red), lineNumber};
      if (c.text.empty()) syntax(lineNumber, "text content cannot be empty");
      c.values = {number(t[1], lineNumber), number(t[2], lineNumber),
                  number(t[red], lineNumber), number(t[red + 1], lineNumber), number(t[red + 2], lineNumber)};
      validateColor(c.values, 2, lineNumber);
      p.commands.push_back(std::move(c));
    } else if (op == "run") {
      expectCount(t, 0, lineNumber, op);
      run = true;
    } else {
      syntax(lineNumber, "unknown instruction '" + op + "'");
    }
  }
  if (!header) syntax(1, "missing header 'bygl 1'");
  if (!run) syntax(lineNumber == 0 ? 1 : lineNumber, "program must end with 'run'");
  if (!p.hasWindow) syntax(1, "program must declare a window");
  return p;
}

std::string cppString(const std::string& s) {
  std::ostringstream out;
  out << '"';
  for (unsigned char c : s) {
    switch (c) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 32 || c >= 127) {
          out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << std::dec;
        } else out << static_cast<char>(c);
    }
  }
  out << '"';
  return out.str();
}

std::string f(double value) {
  std::ostringstream out;
  out << std::setprecision(9) << value;
  std::string result = out.str();
  if (result.find_first_of(".eE") == std::string::npos) result += ".0";
  return result;
}

std::string emit(const Program& p) {
  std::ostringstream o;
  o << R"CPP(#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif
#include <array>
#include <cmath>
#include <cctype>
#include <iostream>
#include <string>

namespace {
constexpr double PI = 3.14159265358979323846;
struct V3 { float x,y,z; };
V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 norm(V3 a){float n=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); if(n<1e-6f)return {0,0,1}; return {a.x/n,a.y/n,a.z/n};}
float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
void camera(float ex,float ey,float ez,float tx,float ty,float tz,int w,int h){
  float aspect=static_cast<float>(w)/static_cast<float>(h>0?h:1);
  float nearZ=0.1f, farZ=500.0f, top=nearZ*std::tan(45.0f*static_cast<float>(PI)/360.0f);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glFrustum(-top*aspect,top*aspect,-top,top,nearZ,farZ);
  V3 eye{ex,ey,ez}, center{tx,ty,tz}, forward=norm(sub(center,eye));
  V3 side=norm(cross(forward,{0,1,0})); V3 up=cross(side,forward);
  GLfloat m[16]={side.x,up.x,-forward.x,0, side.y,up.y,-forward.y,0,
                 side.z,up.z,-forward.z,0, -dot(side,eye),-dot(up,eye),dot(forward,eye),1};
  glMatrixMode(GL_MODELVIEW); glLoadMatrixf(m);
}
void color(float r,float g,float b){glColor3f(r,g,b);}
void grid(int divisions,float spacing,float r,float g,float b){
  glDisable(GL_LIGHTING); color(r,g,b); glBegin(GL_LINES);
  for(int i=-divisions;i<=divisions;++i){float p=i*spacing; glVertex3f(p,0,-divisions*spacing);glVertex3f(p,0,divisions*spacing);glVertex3f(-divisions*spacing,0,p);glVertex3f(divisions*spacing,0,p);}
  glEnd(); glEnable(GL_LIGHTING);
}
void cube(float x,float y,float z,float size,float r,float g,float b){
  float h=size*0.5f; color(r,g,b); glPushMatrix(); glTranslatef(x,y,z); glBegin(GL_QUADS);
  glNormal3f(0,0,1); glVertex3f(-h,-h,h);glVertex3f(h,-h,h);glVertex3f(h,h,h);glVertex3f(-h,h,h);
  glNormal3f(0,0,-1); glVertex3f(h,-h,-h);glVertex3f(-h,-h,-h);glVertex3f(-h,h,-h);glVertex3f(h,h,-h);
  glNormal3f(1,0,0); glVertex3f(h,-h,h);glVertex3f(h,-h,-h);glVertex3f(h,h,-h);glVertex3f(h,h,h);
  glNormal3f(-1,0,0); glVertex3f(-h,-h,-h);glVertex3f(-h,-h,h);glVertex3f(-h,h,h);glVertex3f(-h,h,-h);
  glNormal3f(0,1,0); glVertex3f(-h,h,h);glVertex3f(h,h,h);glVertex3f(h,h,-h);glVertex3f(-h,h,-h);
  glNormal3f(0,-1,0); glVertex3f(-h,-h,-h);glVertex3f(h,-h,-h);glVertex3f(h,-h,h);glVertex3f(-h,-h,h);
  glEnd(); glPopMatrix();
}
void sphere(float x,float y,float z,float radius,float r,float g,float b){
  color(r,g,b); glPushMatrix(); glTranslatef(x,y,z);
  const int stacks=16,slices=24;
  for(int i=0;i<stacks;++i){float a0=-static_cast<float>(PI/2)+static_cast<float>(PI)*i/stacks;float a1=-static_cast<float>(PI/2)+static_cast<float>(PI)*(i+1)/stacks;glBegin(GL_QUAD_STRIP);
    for(int j=0;j<=slices;++j){float lon=static_cast<float>(2*PI)*j/slices;for(float lat:{a0,a1}){float nx=std::cos(lat)*std::cos(lon),ny=std::sin(lat),nz=std::cos(lat)*std::sin(lon);glNormal3f(nx,ny,nz);glVertex3f(radius*nx,radius*ny,radius*nz);}}
    glEnd();}
  glPopMatrix();
}
void line3(float x1,float y1,float z1,float x2,float y2,float z2,float r,float g,float b){glDisable(GL_LIGHTING);color(r,g,b);glBegin(GL_LINES);glVertex3f(x1,y1,z1);glVertex3f(x2,y2,z2);glEnd();glEnable(GL_LIGHTING);}
std::array<unsigned char,7> glyph(char c){
  if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
  switch(c){
    case 'A':return {14,17,17,31,17,17,17}; case 'B':return {30,17,17,30,17,17,30};
    case 'C':return {14,17,16,16,16,17,14}; case 'D':return {30,17,17,17,17,17,30};
    case 'E':return {31,16,16,30,16,16,31}; case 'F':return {31,16,16,30,16,16,16};
    case 'G':return {14,17,16,23,17,17,15}; case 'H':return {17,17,17,31,17,17,17};
    case 'I':return {14,4,4,4,4,4,14}; case 'J':return {7,2,2,2,18,18,12};
    case 'K':return {17,18,20,24,20,18,17}; case 'L':return {16,16,16,16,16,16,31};
    case 'M':return {17,27,21,21,17,17,17}; case 'N':return {17,25,21,19,17,17,17};
    case 'O':return {14,17,17,17,17,17,14}; case 'P':return {30,17,17,30,16,16,16};
    case 'Q':return {14,17,17,17,21,18,13}; case 'R':return {30,17,17,30,20,18,17};
    case 'S':return {15,16,16,14,1,1,30}; case 'T':return {31,4,4,4,4,4,4};
    case 'U':return {17,17,17,17,17,17,14}; case 'V':return {17,17,17,17,17,10,4};
    case 'W':return {17,17,17,21,21,21,10}; case 'X':return {17,17,10,4,10,17,17};
    case 'Y':return {17,17,10,4,4,4,4}; case 'Z':return {31,1,2,4,8,16,31};
    case '0':return {14,17,19,21,25,17,14}; case '1':return {4,12,4,4,4,4,14};
    case '2':return {14,17,1,2,4,8,31}; case '3':return {30,1,1,14,1,1,30};
    case '4':return {2,6,10,18,31,2,2}; case '5':return {31,16,16,30,1,1,30};
    case '6':return {14,16,16,30,17,17,14}; case '7':return {31,1,2,4,8,8,8};
    case '8':return {14,17,17,14,17,17,14}; case '9':return {14,17,17,15,1,1,14};
    case '.':return {0,0,0,0,0,12,12}; case ',':return {0,0,0,0,4,4,8};
    case ':':return {0,12,12,0,12,12,0}; case '-':return {0,0,0,31,0,0,0};
    case '_':return {0,0,0,0,0,0,31}; case '!':return {4,4,4,4,4,0,4};
    case '?':return {14,17,1,2,4,0,4}; case '/':return {1,2,2,4,8,8,16};
    case '+':return {0,4,4,31,4,4,0}; case ' ':return {0,0,0,0,0,0,0};
    default:return {31,17,21,21,21,17,31};
  }
}
void rect2(float x,float y,float w,float h,float r,float g,float b){color(r,g,b);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();}
void circle2(float x,float y,float radius,float r,float g,float b){color(r,g,b);glBegin(GL_TRIANGLE_FAN);glVertex2f(x,y);for(int i=0;i<=48;++i){float a=static_cast<float>(2*PI)*i/48;glVertex2f(x+std::cos(a)*radius,y+std::sin(a)*radius);}glEnd();}
void text2(float x,float y,const std::string& s,float scale,float r,float g,float b){color(r,g,b);glBegin(GL_QUADS);float cursor=x;for(char c:s){auto rows=glyph(c);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1<<(4-col))){float px=cursor+col*scale,py=y+row*scale;glVertex2f(px,py);glVertex2f(px+scale,py);glVertex2f(px+scale,py+scale);glVertex2f(px,py+scale);}cursor+=6*scale;}glEnd();}
void overlay(int w,int h){glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,w,h,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
)CPP";

  for (const auto& c : p.commands) {
    const auto& v = c.values;
    switch (c.kind) {
      case Kind::Grid: break;
      case Kind::Cube: break;
      case Kind::Sphere: break;
      case Kind::Line3: break;
      case Kind::Rotate: break;
      case Kind::Rect:
        o << "rect2(" << f(v[0]) << "f," << f(v[1]) << "f," << f(v[2]) << "f," << f(v[3]) << "f," << f(v[4]) << "f," << f(v[5]) << "f," << f(v[6]) << "f);\n";
        break;
      case Kind::Circle:
        o << "circle2(" << f(v[0]) << "f," << f(v[1]) << "f," << f(v[2]) << "f," << f(v[3]) << "f," << f(v[4]) << "f," << f(v[5]) << "f);\n";
        break;
      case Kind::Text:
        o << "text2(" << f(v[0]) << "f," << f(v[1]) << "f," << cppString(c.text) << ",2.0f," << f(v[2]) << "f," << f(v[3]) << "f," << f(v[4]) << "f);\n";
        break;
    }
  }
  o << R"CPP(glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glEnable(GL_DEPTH_TEST);}
void frame(int w,int h,double t){
  glViewport(0,0,w,h);
  glClearColor()CPP" << f(p.clear[0]) << "f," << f(p.clear[1]) << "f," << f(p.clear[2]) << R"CPP(f,1.0f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  camera()CPP" << f(p.camera[0]) << "f," << f(p.camera[1]) << "f," << f(p.camera[2]) << "f," << f(p.camera[3]) << "f," << f(p.camera[4]) << "f," << f(p.camera[5]) << "f" << R"CPP(,w,h);
  glEnable(GL_DEPTH_TEST);glEnable(GL_NORMALIZE);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);glShadeModel(GL_SMOOTH);
  GLfloat lightPos[4]={-4.0f,8.0f,6.0f,1.0f};GLfloat lightAmbient[4]={0.25f,0.25f,0.28f,1.0f};GLfloat lightDiffuse[4]={0.9f,0.9f,0.9f,1.0f};
  glLightfv(GL_LIGHT0,GL_POSITION,lightPos);glLightfv(GL_LIGHT0,GL_AMBIENT,lightAmbient);glLightfv(GL_LIGHT0,GL_DIFFUSE,lightDiffuse);glPushMatrix();
)CPP";
  if (p.hasRotation) {
    o << "glRotatef(static_cast<float>(t*" << f(p.rotation[0]) << "),1,0,0);\n";
    o << "glRotatef(static_cast<float>(t*" << f(p.rotation[1]) << "),0,1,0);\n";
    o << "glRotatef(static_cast<float>(t*" << f(p.rotation[2]) << "),0,0,1);\n";
  }
  for (const auto& c : p.commands) {
    const auto& v = c.values;
    switch (c.kind) {
      case Kind::Grid:
        o << "grid(" << static_cast<int>(v[0]) << "," << f(v[1]) << "f," << f(v[2]) << "f," << f(v[3]) << "f," << f(v[4]) << "f);\n";
        break;
      case Kind::Cube:
        o << "cube(" << f(v[0]) << "f," << f(v[1]) << "f," << f(v[2]) << "f," << f(v[3]) << "f," << f(v[4]) << "f," << f(v[5]) << "f," << f(v[6]) << "f);\n";
        break;
      case Kind::Sphere:
        o << "sphere(" << f(v[0]) << "f," << f(v[1]) << "f," << f(v[2]) << "f," << f(v[3]) << "f," << f(v[4]) << "f," << f(v[5]) << "f," << f(v[6]) << "f);\n";
        break;
      case Kind::Line3:
        o << "line3(";
        for (size_t i = 0; i < 9; ++i) { if (i) o << ','; o << f(v[i]) << 'f'; }
        o << ");\n";
        break;
      case Kind::Rotate: case Kind::Rect: case Kind::Circle: case Kind::Text: break;
    }
  }
  o << R"CPP(glPopMatrix();overlay(w,h);}
} // namespace
#if defined(_WIN32)
namespace {
LRESULT CALLBACK byglWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcA(hwnd, message, wParam, lParam);
}
}
int main(){
  HINSTANCE instance=GetModuleHandleA(nullptr);
  const char* className="BYGLNativeWindow";
  WNDCLASSA wc{}; wc.style=CS_OWNDC; wc.lpfnWndProc=byglWindowProc; wc.hInstance=instance;
  wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.lpszClassName=className;
  if(!RegisterClassA(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){std::cerr<<"BYGL: RegisterClass failed.\n";return 2;}
  DWORD style=WS_OVERLAPPEDWINDOW; RECT bounds{0,0,)CPP" << p.width << ',' << p.height << R"CPP(};
  AdjustWindowRect(&bounds,style,FALSE);
  HWND window=CreateWindowA(className,)CPP" << cppString(p.title) << R"CPP(,style,CW_USEDEFAULT,CW_USEDEFAULT,
      bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
  if(!window){std::cerr<<"BYGL: CreateWindow failed.\n";return 3;}
  HDC device=GetDC(window);
  PIXELFORMATDESCRIPTOR format{}; format.nSize=sizeof(format); format.nVersion=1;
  format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
  format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=24; format.cDepthBits=24; format.iLayerType=PFD_MAIN_PLANE;
  int pixelFormat=ChoosePixelFormat(device,&format);
  if(!pixelFormat || !SetPixelFormat(device,pixelFormat,&format)){std::cerr<<"BYGL: pixel format setup failed.\n";ReleaseDC(window,device);DestroyWindow(window);return 4;}
  HGLRC context=wglCreateContext(device);
  if(!context || !wglMakeCurrent(device,context)){std::cerr<<"BYGL: OpenGL context creation failed.\n";if(context)wglDeleteContext(context);ReleaseDC(window,device);DestroyWindow(window);return 5;}
  ShowWindow(window,SW_SHOW); UpdateWindow(window); glEnable(GL_DEPTH_TEST);
  LARGE_INTEGER frequency{},start{},now{}; QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
  bool running=true;
  while(running){
    MSG message{};
    while(PeekMessageA(&message,nullptr,0,0,PM_REMOVE)){
      if(message.message==WM_QUIT){running=false;break;}
      TranslateMessage(&message); DispatchMessageA(&message);
    }
    if(!running)break;
    if(GetAsyncKeyState(VK_ESCAPE)&0x8000){running=false;break;}
    RECT client{}; GetClientRect(window,&client);
    int w=client.right-client.left,h=client.bottom-client.top;
    QueryPerformanceCounter(&now);
    double elapsed=frequency.QuadPart?static_cast<double>(now.QuadPart-start.QuadPart)/frequency.QuadPart:0.0;
    frame(w,h,elapsed); SwapBuffers(device);
  }
  wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context); ReleaseDC(window,device); DestroyWindow(window); return 0;
}
#else
int main(){
  if(!glfwInit()){std::cerr<<"BYGL: GLFW initialization failed (check your OpenGL/display environment).\n";return 2;}
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
  GLFWwindow* window=glfwCreateWindow()CPP" << p.width << ',' << p.height << ',' << cppString(p.title) << R"CPP(,nullptr,nullptr);
  if(!window){std::cerr<<"BYGL: could not create an OpenGL 2.1 window.\n";glfwTerminate();return 3;}
  glfwMakeContextCurrent(window);glfwSwapInterval(1);glEnable(GL_DEPTH_TEST);
  while(!glfwWindowShouldClose(window)){
    if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS)glfwSetWindowShouldClose(window,1);
    int w=0,h=0;glfwGetFramebufferSize(window,&w,&h);frame(w,h,glfwGetTime());
    glfwSwapBuffers(window);glfwPollEvents();
  }
  glfwDestroyWindow(window);glfwTerminate();return 0;
}
#endif
)CPP";
  return o.str();
}

std::vector<std::string> splitFlags(const std::string& text) {
  std::istringstream in(text);
  std::vector<std::string> out;
  std::string word;
  while (in >> word) out.push_back(std::move(word));
  return out;
}

class TempFile {
 public:
  explicit TempFile(std::filesystem::path path) : path_(std::move(path)) {}
  ~TempFile() { std::error_code ec; std::filesystem::remove(path_, ec); }
  const std::filesystem::path& path() const { return path_; }
 private:
  std::filesystem::path path_;
};

std::string chooseCxx(const std::string& requested) {
  if (!requested.empty()) return requested;
  if (const char* env = std::getenv("CXX"); env && *env) return env;
  for (const char* candidate : {"clang++", "g++", "c++"}) {
    ProcessResult probe = runProcess({candidate, "--version"});
    if (probe.started && probe.exitCode == 0) return candidate;
  }
  throw CompileError(Category::Toolchain, Loc{}, "no C++ compiler found; install clang++ or g++");
}

std::vector<std::string> graphicsFlags() {
#if defined(__APPLE__)
  ProcessResult pkg = runProcess({"pkg-config", "--cflags", "--libs", "glfw3"});
  if (!pkg.started || pkg.exitCode != 0)
    throw CompileError(Category::Toolchain, Loc{}, "BYGL requires GLFW 3 development files and pkg-config; install GLFW and pkg-config");
  auto flags = splitFlags(pkg.output);
  flags.insert(flags.end(), {"-framework", "OpenGL"});
  return flags;
#elif defined(_WIN32)
  // The Windows backend uses native Win32 + WGL, so no GLFW or pkg-config is needed.
  return {"-lopengl32", "-lgdi32", "-luser32"};
#else
  ProcessResult pkg = runProcess({"pkg-config", "--cflags", "--libs", "glfw3", "gl"});
  if (!pkg.started || pkg.exitCode != 0)
    throw CompileError(Category::Toolchain, Loc{},
      "BYGL needs GLFW 3, OpenGL development headers and pkg-config. On Debian/Ubuntu/WSL run:\n"
      "  sudo apt update && sudo apt install build-essential clang cmake pkg-config libglfw3-dev libgl1-mesa-dev");
  return splitFlags(pkg.output);
#endif
}

}  // namespace

std::string compileToCpp(const std::string& source) {
  std::istringstream in(source);
  std::string line;
  while (std::getline(in, line)) {
    auto t = tokenize(line, 1);
    if (t.empty()) continue;
    if (t.size() == 2 && t[0].value == "bygl" && t[1].value == "2")
      return compileToCppV2(source);
    break;
  }
  return emit(parse(source));
}

void buildExecutable(const std::string& source, const std::string& sourceName,
                     const std::string& outputPath, const std::string& cxx) {
  std::string cpp = compileToCpp(source);
  TempFile temp(outputPath + ".bygl.tmp.cpp");
  std::ofstream out(temp.path(), std::ios::binary);
  if (!out) throw CompileError(Category::Toolchain, Loc{}, "cannot write temporary generated C++ source for '" + sourceName + "'");
  out << cpp;
  out.close();

  std::vector<std::string> command{chooseCxx(cxx), "-std=c++17", "-O2", temp.path().string(), "-o", outputPath};
  auto flags = graphicsFlags();
  command.insert(command.end(), flags.begin(), flags.end());
  ProcessResult result = runProcess(command);
  if (!result.started) throw CompileError(Category::Toolchain, Loc{}, "could not start the C++ compiler for BYGL");
  if (result.exitCode != 0) {
    std::error_code ignored;
    std::filesystem::remove(outputPath, ignored);
    throw CompileError(Category::Toolchain, Loc{}, "OpenGL compilation failed (exit code " + std::to_string(result.exitCode) + "):\n" + result.output);
  }
}

}  // namespace bylink::opengl
