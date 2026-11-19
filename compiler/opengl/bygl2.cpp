#include "opengl/bygl2.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "common/diagnostic.h"

namespace bylink::opengl {
namespace {
struct Tok { std::string s; bool quoted=false; };
struct SrcLine { unsigned line=0; std::vector<Tok> t; };
[[noreturn]] void fail(unsigned line,const std::string& msg){
  throw CompileError(Category::Syntax,Loc{line,1},"BYGL 2: "+msg);
}
std::vector<Tok> lexLine(const std::string& s,unsigned line){
  std::vector<Tok> out; size_t i=0;
  while(i<s.size()){
    while(i<s.size()&&std::isspace(static_cast<unsigned char>(s[i])))++i;
    if(i>=s.size()||s[i]=='#'||(s[i]=='/'&&i+1<s.size()&&s[i+1]=='/'))break;
    if(s[i]=='"'){
      ++i; std::string v; bool closed=false;
      while(i<s.size()){
        char c=s[i++]; if(c=='"'){closed=true;break;}
        if(c=='\\'&&i<s.size()){char e=s[i++];switch(e){case 'n':v+='\n';break;case 't':v+='\t';break;case 'r':v+='\r';break;case '"':v+='"';break;case '\\':v+='\\';break;default:fail(line,"unsupported string escape");}}
        else v+=c;
      }
      if(!closed)fail(line,"unterminated string"); out.push_back({v,true});
    }else{
      size_t b=i;while(i<s.size()&&!std::isspace(static_cast<unsigned char>(s[i])))++i;
      out.push_back({s.substr(b,i-b),false});
    }
  }
  return out;
}
std::string join(const std::vector<Tok>& t,size_t a,size_t b){
  std::string s;for(size_t i=a;i<b;++i){if(i>a)s+=' ';s+=t[i].s;}
  const std::pair<std::string,std::string> q[]={{"\xE2\x80\x9C","\xE2\x80\x9D"},{"\xE2\x80\x98","\xE2\x80\x99"},{"\"","\""}};
  for(const auto& p:q)if(s.size()>=p.first.size()+p.second.size()&&s.compare(0,p.first.size(),p.first)==0&&s.compare(s.size()-p.second.size(),p.second.size(),p.second)==0){s=s.substr(p.first.size(),s.size()-p.first.size()-p.second.size());break;}
  return s;
}
bool ident(const std::string& s){if(s.empty()||!(std::isalpha((unsigned char)s[0])||s[0]=='_'))return false;for(char c:s)if(!(std::isalnum((unsigned char)c)||c=='_'))return false;return true;}
double num(const Tok& t,unsigned line){if(t.quoted)fail(line,"expected number");size_t n=0;double v=0;try{v=std::stod(t.s,&n);}catch(...){fail(line,"invalid number '"+t.s+"'");}if(n!=t.s.size()||!std::isfinite(v))fail(line,"invalid finite number '"+t.s+"'");return v;}
std::string fmt(double x){std::ostringstream o;o<<std::setprecision(10)<<x;auto s=o.str();if(s.find_first_of(".eE")==std::string::npos)s+=".0";return s;}

struct Expr {
  enum class K{Number,Var,Dt,Time,Key,Add,Sub,Mul,Div,Mod,Neg,Abs,Min,Max,Lt,Le,Gt,Ge,Eq,Ne,And,Or,Not};
  K k=K::Number; double n=0; std::string name; std::shared_ptr<Expr> a,b;
};
using EP=std::shared_ptr<Expr>;
const std::map<std::string,Expr::K> ops={{"add",Expr::K::Add},{"sub",Expr::K::Sub},{"mul",Expr::K::Mul},{"div",Expr::K::Div},{"mod",Expr::K::Mod},{"neg",Expr::K::Neg},{"abs",Expr::K::Abs},{"min",Expr::K::Min},{"max",Expr::K::Max},{"lt",Expr::K::Lt},{"le",Expr::K::Le},{"gt",Expr::K::Gt},{"ge",Expr::K::Ge},{"eq",Expr::K::Eq},{"ne",Expr::K::Ne},{"and",Expr::K::And},{"or",Expr::K::Or},{"not",Expr::K::Not}};
bool unary(Expr::K k){return k==Expr::K::Neg||k==Expr::K::Abs||k==Expr::K::Not;}
std::set<std::string> keys={"W","A","S","D","UP","DOWN","LEFT","RIGHT","SPACE","ENTER","ESC","R"};
EP parseExpr(const std::vector<Tok>& t,size_t& pos,unsigned line,const std::set<std::string>& vars){
  if(pos>=t.size())fail(line,"missing expression");
  const Tok tok=t[pos++]; if(tok.quoted)fail(line,"strings are not valid expressions");
  auto e=std::make_shared<Expr>();
  if(tok.s=="dt"){e->k=Expr::K::Dt;return e;} if(tok.s=="time"){e->k=Expr::K::Time;return e;}
  if(tok.s=="key"){
    if(pos>=t.size())fail(line,"key expects a key name");std::string k=t[pos++].s;
    if(!keys.count(k))fail(line,"unknown key '"+k+"'");e->k=Expr::K::Key;e->name=k;return e;
  }
  auto it=ops.find(tok.s);if(it!=ops.end()){e->k=it->second;e->a=parseExpr(t,pos,line,vars);if(!unary(e->k))e->b=parseExpr(t,pos,line,vars);return e;}
  size_t used=0;double v=0;try{v=std::stod(tok.s,&used);}catch(...){}
  if(used==tok.s.size()&&!tok.s.empty()){if(!std::isfinite(v))fail(line,"non-finite number");e->k=Expr::K::Number;e->n=v;return e;}
  if(!ident(tok.s)||!vars.count(tok.s))fail(line,"unknown variable or expression token '"+tok.s+"'");e->k=Expr::K::Var;e->name=tok.s;return e;
}
EP parseFullExpr(const std::vector<Tok>& t,size_t pos,unsigned line,const std::set<std::string>& vars){EP e=parseExpr(t,pos,line,vars);if(pos!=t.size())fail(line,"unexpected token '"+t[pos].s+"' after expression");return e;}
EP parseNumExpr(const std::vector<Tok>& t,size_t& pos,unsigned line,const std::set<std::string>& vars){return parseExpr(t,pos,line,vars);}
std::string keyCode(const std::string& k){if(k=="UP")return "1001";if(k=="DOWN")return "1002";if(k=="LEFT")return "1003";if(k=="RIGHT")return "1004";if(k=="SPACE")return "1005";if(k=="ENTER")return "1006";if(k=="ESC")return "1000";return std::to_string((int)k[0]);}
std::string exprCpp(const EP& e){
  auto a=[&](){return exprCpp(e->a);};auto b=[&](){return exprCpp(e->b);};
  switch(e->k){case Expr::K::Number:return fmt(e->n);case Expr::K::Var:return "s."+e->name;case Expr::K::Dt:return "dt";case Expr::K::Time:return "time";case Expr::K::Key:return "keyDown("+keyCode(e->name)+")?1.0:0.0";
  case Expr::K::Add:return "("+a()+"+"+b()+")";case Expr::K::Sub:return "("+a()+"-"+b()+")";case Expr::K::Mul:return "("+a()+"*"+b()+")";case Expr::K::Div:return "safeDiv("+a()+","+b()+")";case Expr::K::Mod:return "safeMod("+a()+","+b()+")";case Expr::K::Neg:return "(-("+a()+"))";case Expr::K::Abs:return "std::fabs("+a()+")";case Expr::K::Min:return "std::min("+a()+","+b()+")";case Expr::K::Max:return "std::max("+a()+","+b()+")";
  case Expr::K::Lt:return "(("+a()+")<("+b()+"))";case Expr::K::Le:return "(("+a()+")<=("+b()+"))";case Expr::K::Gt:return "(("+a()+")>("+b()+"))";case Expr::K::Ge:return "(("+a()+")>=("+b()+"))";case Expr::K::Eq:return "(("+a()+")==("+b()+"))";case Expr::K::Ne:return "(("+a()+")!=("+b()+"))";case Expr::K::And:return "(("+a()+")!=0.0 && ("+b()+")!=0.0)";case Expr::K::Or:return "(("+a()+")!=0.0 || ("+b()+")!=0.0)";case Expr::K::Not:return "(("+a()+")==0.0)";}
  return "0.0";
}
struct Stmt{enum class K{Set,Add,If};K k;std::string name;EP e;std::vector<Stmt> yes,no;unsigned line=0;};
struct Var{std::string name;double init=0;};
struct Draw{std::string op,text;std::vector<EP> args;unsigned line=0;};
struct Program{int w=960,h=640;std::string title="BYlink Game";std::array<double,3> clear{0.02,0.025,0.05};std::array<double,6> camera{0,8,12,0,0,0};bool window=false,cameraSet=false;std::vector<Var> vars;std::set<std::string> names;std::vector<Stmt> update;std::vector<Draw> draw;};

std::vector<Stmt> parseBlock(const std::vector<SrcLine>& ls,size_t& i,Program& p,const std::string& stop){
  std::vector<Stmt> out;
  while(i<ls.size()){
    const auto& l=ls[i];const auto& t=l.t;const std::string& op=t[0].s;
    if(op==stop||op=="else"||op=="end")return out;
    if(op=="set"||op=="add"){
      if(t.size()<3)fail(l.line,op+" expects a variable and expression");if(!p.names.count(t[1].s))fail(l.line,"unknown variable '"+t[1].s+"'");
      Stmt s;s.k=op=="set"?Stmt::K::Set:Stmt::K::Add;s.name=t[1].s;s.e=parseFullExpr(t,2,l.line,p.names);s.line=l.line;out.push_back(std::move(s));++i;continue;
    }
    if(op=="if"){
      if(t.size()<2)fail(l.line,"if expects an expression");Stmt s;s.k=Stmt::K::If;s.e=parseFullExpr(t,1,l.line,p.names);s.line=l.line;++i;
      s.yes=parseBlock(ls,i,p,"__never__");
      if(i>=ls.size())fail(l.line,"if block is missing end");
      if(ls[i].t[0].s=="else"){++i;s.no=parseBlock(ls,i,p,"__never__");if(i>=ls.size()||ls[i].t[0].s!="end")fail(l.line,"else block is missing end");}
      else if(ls[i].t[0].s!="end")fail(l.line,"if block is missing end");
      ++i;out.push_back(std::move(s));continue;
    }
    fail(l.line,"only set, add, if, else and end are valid inside update");
  }
  return out;
}
void emitStmts(std::ostringstream& o,const std::vector<Stmt>& ss,int depth=1){
  const std::string pad(static_cast<size_t>(depth)*2,' ');
  for(const auto& s:ss){if(s.k==Stmt::K::Set)o<<pad<<"s."<<s.name<<"="<<exprCpp(s.e)<<";\n";else if(s.k==Stmt::K::Add)o<<pad<<"s."<<s.name<<"+="<<exprCpp(s.e)<<";\n";else{o<<pad<<"if(("<<exprCpp(s.e)<<")!=0.0){\n";emitStmts(o,s.yes,depth+1);if(!s.no.empty()){o<<pad<<"}else{\n";emitStmts(o,s.no,depth+1);}o<<pad<<"}\n";}}
}

Draw parseDraw(const SrcLine& l,Program& p){
  const auto& t=l.t;Draw d;d.op=t[0].s;d.line=l.line;
  size_t count=0;
  if(d.op=="grid")count=5;else if(d.op=="cube"||d.op=="sphere")count=7;else if(d.op=="box"||d.op=="line3")count=9;else if(d.op=="rect")count=7;else if(d.op=="circle")count=6;else if(d.op=="label")count=6;
  else if(d.op=="text"){
    if(t.size()<7)fail(l.line,"text expects x y string r g b");
    size_t pos=1;d.args.push_back(parseNumExpr(t,pos,l.line,p.names));d.args.push_back(parseNumExpr(t,pos,l.line,p.names));
    const size_t colorStart=t.size()-3;d.text=join(t,pos,colorStart);if(d.text.empty())fail(l.line,"text string cannot be empty");
    for(size_t j=colorStart;j<t.size();++j){size_t q=j;d.args.push_back(parseNumExpr(t,q,l.line,p.names));if(q!=j+1)fail(l.line,"text colors must be numeric");}
    return d;
  }else fail(l.line,"unknown draw instruction '"+d.op+"'");
  size_t pos=1;
  for(size_t n=0;n<count;++n)d.args.push_back(parseNumExpr(t,pos,l.line,p.names));
  // Circle accepts RGB (6 operands) and optional RGBA (7 operands).
  // Several code models naturally emit an alpha channel even when RGB is sufficient.
  if(d.op=="circle" && pos<t.size()) {
    d.args.push_back(parseNumExpr(t,pos,l.line,p.names));
  }
  if(pos!=t.size()) {
    const size_t got=t.size()-1;
    if(d.op=="circle") fail(l.line,"circle expects x y radius r g b [a] (6 or 7 values), got "+std::to_string(got));
    fail(l.line,"too many arguments for '"+d.op+"' (expected "+std::to_string(count)+", got "+std::to_string(got)+")");
  }
  return d;
}
Program parseV2(const std::string& source){
  std::vector<SrcLine> ls;std::istringstream in(source);std::string line;unsigned n=0;while(std::getline(in,line)){++n;auto t=lexLine(line,n);if(!t.empty())ls.push_back({n,std::move(t)});}
  if(ls.empty()||ls[0].t.size()!=2||ls[0].t[0].s!="bygl"||ls[0].t[1].s!="2")fail(1,"expected header 'bygl 2'");
  Program p;size_t i=1;bool update=false,draw=false,run=false;
  while(i<ls.size()){
    const auto& l=ls[i];const auto& t=l.t;const auto& op=t[0].s;
    if(op=="window"){
      if(p.window||t.size()<4)fail(l.line,"window expects width height title and may appear once");double w=num(t[1],l.line),h=num(t[2],l.line);if(w<64||w>8192||h<64||h>8192||std::floor(w)!=w||std::floor(h)!=h)fail(l.line,"window size must be integer 64..8192");p.w=(int)w;p.h=(int)h;p.title=join(t,3,t.size());p.window=true;
    }else if(op=="clear"){
      if(t.size()!=4)fail(l.line,"clear expects RGB");for(int k=0;k<3;++k){p.clear[k]=num(t[k+1],l.line);if(p.clear[k]<0||p.clear[k]>1)fail(l.line,"color must be 0..1");}
    }else if(op=="camera"){
      if(t.size()!=7)fail(l.line,"camera expects six coordinates");for(int k=0;k<6;++k)p.camera[k]=num(t[k+1],l.line);p.cameraSet=true;
    }else if(op=="var"){
      if(update||draw||t.size()!=3||!ident(t[1].s))fail(l.line,"var syntax: var NAME INITIAL_VALUE (before update)");if(p.names.count(t[1].s)||t[1].s=="dt"||t[1].s=="time")fail(l.line,"duplicate or reserved variable '"+t[1].s+"'");double v=num(t[2],l.line);p.names.insert(t[1].s);p.vars.push_back({t[1].s,v});
    }else if(op=="update"){
      if(update||draw||run||t.size()!=1)fail(l.line,"update must appear once before draw");update=true;++i;p.update=parseBlock(ls,i,p,"draw");if(i>=ls.size()||ls[i].t[0].s!="draw")fail(l.line,"update block must be followed by draw");continue;
    }else if(op=="draw"){
      if(!update||draw||run||t.size()!=1)fail(l.line,"draw must follow update exactly once");draw=true;++i;while(i<ls.size()&&ls[i].t[0].s!="run"){p.draw.push_back(parseDraw(ls[i],p));++i;}if(i>=ls.size())fail(l.line,"draw block must end with run");continue;
    }else if(op=="run"){
      if(t.size()!=1||!draw)fail(l.line,"run must terminate the draw block");run=true;++i;if(i!=ls.size())fail(ls[i].line,"no instructions allowed after run");break;
    }else fail(l.line,"unexpected instruction '"+op+"' outside update/draw blocks");
    ++i;
  }
  if(!p.window)fail(1,"program requires window");if(!update||!draw||!run)fail(ls.empty()?1:ls.back().line,"program requires update, draw and run");return p;
}

std::string cppStr(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='\\')o<<"\\\\";else if(c=='"')o<<"\\\"";else if(c=='\n')o<<"\\n";else if(c=='\r')o<<"\\r";else if(c=='\t')o<<"\\t";else if(c<32)o<<' ';else o<<(char)c;}o<<'"';return o.str();}
std::string farg(const EP& e){return "static_cast<float>("+exprCpp(e)+")";}
void emitDraw3D(std::ostringstream& o,const Draw& d){auto a=[&](size_t i){return farg(d.args[i]);};const auto& op=d.op;
  if(op=="grid")o<<"gridDraw(static_cast<int>("<<exprCpp(d.args[0])<<"),"<<a(1)<<","<<a(2)<<","<<a(3)<<","<<a(4)<<");\n";
  else if(op=="cube")o<<"cubeDraw("<<a(0)<<","<<a(1)<<","<<a(2)<<","<<a(3)<<","<<a(4)<<","<<a(5)<<","<<a(6)<<");\n";
  else if(op=="sphere")o<<"sphereDraw("<<a(0)<<","<<a(1)<<","<<a(2)<<","<<a(3)<<","<<a(4)<<","<<a(5)<<","<<a(6)<<");\n";
  else if(op=="box")o<<"boxDraw("<<a(0)<<","<<a(1)<<","<<a(2)<<","<<a(3)<<","<<a(4)<<","<<a(5)<<","<<a(6)<<","<<a(7)<<","<<a(8)<<");\n";
  else if(op=="line3")o<<"lineDraw("<<a(0)<<","<<a(1)<<","<<a(2)<<","<<a(3)<<","<<a(4)<<","<<a(5)<<","<<a(6)<<","<<a(7)<<","<<a(8)<<");\n";
}
void emitDraw2D(std::ostringstream& o,const Draw& d){auto a=[&](size_t i){return farg(d.args[i]);};const auto& op=d.op;
  if(op=="rect")o<<"rectDraw("<<a(0)<<","<<a(1)<<","<<a(2)<<","<<a(3)<<","<<a(4)<<","<<a(5)<<","<<a(6)<<");\n";
  else if(op=="circle")o<<"circleDraw("<<a(0)<<","<<a(1)<<","<<a(2)<<","<<a(3)<<","<<a(4)<<","<<a(5)<<","<<(d.args.size()>6?a(6):"1.0f")<<");\n";
  else if(op=="text")o<<"textDraw("<<a(0)<<","<<a(1)<<","<<cppStr(d.text)<<",2.0f,"<<a(2)<<","<<a(3)<<","<<a(4)<<");\n";
  else if(op=="label")o<<"textDraw("<<a(0)<<","<<a(1)<<",numberText("<<exprCpp(d.args[2])<<"),3.0f,"<<a(3)<<","<<a(4)<<","<<a(5)<<");\n";
}

std::string emitV2(const Program& p){std::ostringstream o;
  o<<R"CPP(#if defined(_WIN32)
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
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

namespace {
constexpr double PI=3.14159265358979323846;
struct State {
)CPP";
  for(const auto& v:p.vars)o<<" double "<<v.name<<"="<<fmt(v.init)<<";\n";
  o<<R"CPP(};
double safeDiv(double a,double b){return std::fabs(b)<1e-12?0.0:a/b;}
double safeMod(double a,double b){return std::fabs(b)<1e-12?0.0:std::fmod(a,b);}
#if defined(_WIN32)
bool keyDown(int k){int vk=k;switch(k){case 1000:vk=VK_ESCAPE;break;case 1001:vk=VK_UP;break;case 1002:vk=VK_DOWN;break;case 1003:vk=VK_LEFT;break;case 1004:vk=VK_RIGHT;break;case 1005:vk=VK_SPACE;break;case 1006:vk=VK_RETURN;break;}return (GetAsyncKeyState(vk)&0x8000)!=0;}
#else
GLFWwindow* g_window=nullptr;
bool keyDown(int k){int key=k;switch(k){case 1000:key=GLFW_KEY_ESCAPE;break;case 1001:key=GLFW_KEY_UP;break;case 1002:key=GLFW_KEY_DOWN;break;case 1003:key=GLFW_KEY_LEFT;break;case 1004:key=GLFW_KEY_RIGHT;break;case 1005:key=GLFW_KEY_SPACE;break;case 1006:key=GLFW_KEY_ENTER;break;}return g_window&&glfwGetKey(g_window,key)==GLFW_PRESS;}
#endif
struct V3{float x,y,z;};
V3 sub3(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};} V3 cross3(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 norm3(V3 a){float n=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return n<1e-6f?V3{0,0,1}:V3{a.x/n,a.y/n,a.z/n};} float dot3(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
void setCamera(float ex,float ey,float ez,float tx,float ty,float tz,int w,int h){float asp=float(w)/float(h>0?h:1),nz=.1f,top=nz*std::tan(45.0f*float(PI)/360.0f);glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-top*asp,top*asp,-top,top,nz,500);V3 eye{ex,ey,ez},f=norm3(sub3({tx,ty,tz},eye)),side=norm3(cross3(f,{0,1,0})),up=cross3(side,f);GLfloat m[16]={side.x,up.x,-f.x,0,side.y,up.y,-f.y,0,side.z,up.z,-f.z,0,-dot3(side,eye),-dot3(up,eye),dot3(f,eye),1};glMatrixMode(GL_MODELVIEW);glLoadMatrixf(m);}
void gridDraw(int n,float sp,float r,float g,float b){glDisable(GL_LIGHTING);glColor3f(r,g,b);glBegin(GL_LINES);for(int i=-n;i<=n;++i){float q=i*sp;glVertex3f(q,0,-n*sp);glVertex3f(q,0,n*sp);glVertex3f(-n*sp,0,q);glVertex3f(n*sp,0,q);}glEnd();glEnable(GL_LIGHTING);}
void boxDraw(float x,float y,float z,float sx,float sy,float sz,float r,float g,float b){float a=sx*.5f,c=sy*.5f,d=sz*.5f;glColor3f(r,g,b);glPushMatrix();glTranslatef(x,y,z);glBegin(GL_QUADS);
glNormal3f(0,0,1);glVertex3f(-a,-c,d);glVertex3f(a,-c,d);glVertex3f(a,c,d);glVertex3f(-a,c,d);
glNormal3f(0,0,-1);glVertex3f(a,-c,-d);glVertex3f(-a,-c,-d);glVertex3f(-a,c,-d);glVertex3f(a,c,-d);
glNormal3f(1,0,0);glVertex3f(a,-c,d);glVertex3f(a,-c,-d);glVertex3f(a,c,-d);glVertex3f(a,c,d);
glNormal3f(-1,0,0);glVertex3f(-a,-c,-d);glVertex3f(-a,-c,d);glVertex3f(-a,c,d);glVertex3f(-a,c,-d);
glNormal3f(0,1,0);glVertex3f(-a,c,d);glVertex3f(a,c,d);glVertex3f(a,c,-d);glVertex3f(-a,c,-d);
glNormal3f(0,-1,0);glVertex3f(-a,-c,-d);glVertex3f(a,-c,-d);glVertex3f(a,-c,d);glVertex3f(-a,-c,d);glEnd();glPopMatrix();}
void cubeDraw(float x,float y,float z,float s,float r,float g,float b){boxDraw(x,y,z,s,s,s,r,g,b);}
void sphereDraw(float x,float y,float z,float rad,float r,float g,float b){glColor3f(r,g,b);glPushMatrix();glTranslatef(x,y,z);const int rings=14,seg=20;for(int i=0;i<rings;++i){float a0=-float(PI)/2+float(PI)*i/rings,a1=-float(PI)/2+float(PI)*(i+1)/rings;glBegin(GL_QUAD_STRIP);for(int j=0;j<=seg;++j){float q=2*float(PI)*j/seg;for(float a:{a0,a1}){float nx=std::cos(a)*std::cos(q),ny=std::sin(a),nz=std::cos(a)*std::sin(q);glNormal3f(nx,ny,nz);glVertex3f(rad*nx,rad*ny,rad*nz);}}glEnd();}glPopMatrix();}
void lineDraw(float x1,float y1,float z1,float x2,float y2,float z2,float r,float g,float b){glDisable(GL_LIGHTING);glColor3f(r,g,b);glBegin(GL_LINES);glVertex3f(x1,y1,z1);glVertex3f(x2,y2,z2);glEnd();glEnable(GL_LIGHTING);}
std::array<unsigned char,7> glyph(char c){if(c>='a'&&c<='z')c=char(c-'a'+'A');switch(c){
case 'A':return {14,17,17,31,17,17,17};case 'B':return {30,17,17,30,17,17,30};case 'C':return {14,17,16,16,16,17,14};case 'D':return {30,17,17,17,17,17,30};case 'E':return {31,16,16,30,16,16,31};case 'F':return {31,16,16,30,16,16,16};case 'G':return {14,17,16,23,17,17,15};case 'H':return {17,17,17,31,17,17,17};case 'I':return {14,4,4,4,4,4,14};case 'J':return {7,2,2,2,18,18,12};case 'K':return {17,18,20,24,20,18,17};case 'L':return {16,16,16,16,16,16,31};case 'M':return {17,27,21,21,17,17,17};case 'N':return {17,25,21,19,17,17,17};case 'O':return {14,17,17,17,17,17,14};case 'P':return {30,17,17,30,16,16,16};case 'Q':return {14,17,17,17,21,18,13};case 'R':return {30,17,17,30,20,18,17};case 'S':return {15,16,16,14,1,1,30};case 'T':return {31,4,4,4,4,4,4};case 'U':return {17,17,17,17,17,17,14};case 'V':return {17,17,17,17,17,10,4};case 'W':return {17,17,17,21,21,21,10};case 'X':return {17,17,10,4,10,17,17};case 'Y':return {17,17,10,4,4,4,4};case 'Z':return {31,1,2,4,8,16,31};
case '0':return {14,17,19,21,25,17,14};case '1':return {4,12,4,4,4,4,14};case '2':return {14,17,1,2,4,8,31};case '3':return {30,1,1,14,1,1,30};case '4':return {2,6,10,18,31,2,2};case '5':return {31,16,16,30,1,1,30};case '6':return {14,16,16,30,17,17,14};case '7':return {31,1,2,4,8,8,8};case '8':return {14,17,17,14,17,17,14};case '9':return {14,17,17,15,1,1,14};
case ':':return {0,12,12,0,12,12,0};case '-':return {0,0,0,31,0,0,0};case '.':return {0,0,0,0,0,12,12};case ' ':return {0,0,0,0,0,0,0};default:return {31,17,21,21,21,17,31};}}
void rectDraw(float x,float y,float w,float h,float r,float g,float b){glColor3f(r,g,b);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();}
void circleDraw(float x,float y,float rad,float r,float g,float b,float alpha){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f(r,g,b,alpha);glBegin(GL_TRIANGLE_FAN);glVertex2f(x,y);for(int i=0;i<=48;++i){float a=2*float(PI)*i/48;glVertex2f(x+std::cos(a)*rad,y+std::sin(a)*rad);}glEnd();glDisable(GL_BLEND);}
void textDraw(float x,float y,const std::string& s,float scale,float r,float g,float b){glColor3f(r,g,b);glBegin(GL_QUADS);float cx=x;for(char c:s){auto rows=glyph(c);for(int yy=0;yy<7;++yy)for(int xx=0;xx<5;++xx)if(rows[yy]&(1<<(4-xx))){float px=cx+xx*scale,py=y+yy*scale;glVertex2f(px,py);glVertex2f(px+scale,py);glVertex2f(px+scale,py+scale);glVertex2f(px,py+scale);}cx+=6*scale;}glEnd();}
std::string numberText(double v){std::ostringstream s;s<<std::fixed<<std::setprecision(0)<<v;return s.str();}
)CPP";
  o<<"void update(State& s,double dt,double time){\n";emitStmts(o,p.update);o<<"}\n";
  o<<"void drawScene(int w,int h,double time,const State& s){glViewport(0,0,w,h);glClearColor("<<fmt(p.clear[0])<<"f,"<<fmt(p.clear[1])<<"f,"<<fmt(p.clear[2])<<"f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);\n";
  o<<"setCamera("<<fmt(p.camera[0])<<"f,"<<fmt(p.camera[1])<<"f,"<<fmt(p.camera[2])<<"f,"<<fmt(p.camera[3])<<"f,"<<fmt(p.camera[4])<<"f,"<<fmt(p.camera[5])<<"f,w,h);glEnable(GL_DEPTH_TEST);glEnable(GL_NORMALIZE);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);GLfloat lp[4]={-4,8,6,1},la[4]={.25f,.25f,.28f,1},ld[4]={.9f,.9f,.9f,1};glLightfv(GL_LIGHT0,GL_POSITION,lp);glLightfv(GL_LIGHT0,GL_AMBIENT,la);glLightfv(GL_LIGHT0,GL_DIFFUSE,ld);\n";
  for(const auto& d:p.draw)if(d.op=="grid"||d.op=="cube"||d.op=="box"||d.op=="sphere"||d.op=="line3")emitDraw3D(o,d);
  o<<"glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,w,h,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();\n";
  for(const auto& d:p.draw)if(d.op=="rect"||d.op=="circle"||d.op=="text"||d.op=="label")emitDraw2D(o,d);
  o<<"glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glEnable(GL_DEPTH_TEST);}\n";
  o<<R"CPP(}
#if defined(_WIN32)
namespace {LRESULT CALLBACK bygl2Proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_CLOSE){DestroyWindow(h);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(h,m,w,l);}}
int main(){HINSTANCE inst=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=bygl2Proc;wc.hInstance=inst;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName="BYGL2Window";if(!RegisterClassA(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 2;DWORD style=WS_OVERLAPPEDWINDOW;RECT r{0,0,)CPP"<<p.w<<","<<p.h<<R"CPP(};AdjustWindowRect(&r,style,FALSE);HWND win=CreateWindowA(wc.lpszClassName,)CPP"<<cppStr(p.title)<<R"CPP(,style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,inst,nullptr);if(!win)return 3;HDC dc=GetDC(win);PIXELFORMATDESCRIPTOR pf{};pf.nSize=sizeof(pf);pf.nVersion=1;pf.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pf.iPixelType=PFD_TYPE_RGBA;pf.cColorBits=24;pf.cDepthBits=24;pf.iLayerType=PFD_MAIN_PLANE;int fmt=ChoosePixelFormat(dc,&pf);if(!fmt||!SetPixelFormat(dc,fmt,&pf))return 4;HGLRC ctx=wglCreateContext(dc);if(!ctx||!wglMakeCurrent(dc,ctx))return 5;ShowWindow(win,SW_SHOW);UpdateWindow(win);State state;LARGE_INTEGER freq{},prev{},now{};QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&prev);bool running=true;while(running){MSG msg{};while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageA(&msg);}if(!running||keyDown(1000))break;QueryPerformanceCounter(&now);double dt=freq.QuadPart?double(now.QuadPart-prev.QuadPart)/freq.QuadPart:0;prev=now;dt=std::clamp(dt,0.0,0.05);double tm=freq.QuadPart?double(now.QuadPart)/freq.QuadPart:0;update(state,dt,tm);RECT cr{};GetClientRect(win,&cr);drawScene(cr.right-cr.left,cr.bottom-cr.top,tm,state);SwapBuffers(dc);}wglMakeCurrent(nullptr,nullptr);wglDeleteContext(ctx);ReleaseDC(win,dc);DestroyWindow(win);return 0;}
#else
int main(){if(!glfwInit())return 2;glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);g_window=glfwCreateWindow()CPP"<<p.w<<","<<p.h<<","<<cppStr(p.title)<<R"CPP(,nullptr,nullptr);if(!g_window){glfwTerminate();return 3;}glfwMakeContextCurrent(g_window);glfwSwapInterval(1);State state;double prev=glfwGetTime();while(!glfwWindowShouldClose(g_window)&&!keyDown(1000)){double tm=glfwGetTime(),dt=std::clamp(tm-prev,0.0,0.05);prev=tm;update(state,dt,tm);int w=0,h=0;glfwGetFramebufferSize(g_window,&w,&h);drawScene(w,h,tm,state);glfwSwapBuffers(g_window);glfwPollEvents();}glfwDestroyWindow(g_window);glfwTerminate();return 0;}
#endif
)CPP";
  return o.str();
}
} // namespace

std::string compileToCppV2(const std::string& source){return emitV2(parseV2(source));}
} // namespace bylink::opengl
