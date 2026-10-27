#include <cstdint>
#include <fstream>
#include <string>
#include "effects/combined_renderers.h"
#include "games/game_host.h"
#include "pixel/pixel_canvas.h"

using namespace ledwall;

static void save_matrix(const char* path, const FrameBuffer& frame) {
  std::ofstream out(path, std::ios::binary);
  out << "P6\n30 10\n255\n";
  for (uint8_t y=0; y<10; ++y) for (uint8_t x=0; x<30; ++x) {
    const Rgb p=frame.at(x,y);
    out.put(static_cast<char>(p.r)); out.put(static_cast<char>(p.g)); out.put(static_cast<char>(p.b));
  }
}
static void save_tail(const char* path, const TailFramebuffer& tail) {
  std::ofstream out(path, std::ios::binary);
  out << "P6\n150 1\n255\n";
  for (uint16_t i=0; i<150; ++i) {
    const Rgb p=tail.at(i);
    out.put(static_cast<char>(p.r)); out.put(static_cast<char>(p.g)); out.put(static_cast<char>(p.b));
  }
}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::string base(argv[1]);
  FrameBuffer matrix;
  TailFramebuffer tail; tail.configure(150);
  if (!effects::render_combined("fireworks", matrix, tail, 2750)) return 3;
  save_matrix((base+"/fireworks-2750-matrix.ppm").c_str(), matrix);
  save_tail((base+"/fireworks-2750-tail.ppm").c_str(), tail);

  games::Game* pong = games::game_by_name("pong");
  if (!pong) return 4;
  pong->set_players(1); pong->reset(0); pong->set_phase(games::GamePhase::Running);
  pong->on_step(100);
  matrix.clear(); pong->render(matrix,100);
  save_matrix((base+"/pong-100-matrix.ppm").c_str(), matrix);

  games::Game* tennis = games::game_by_name("tail_tennis");
  if (!tennis) return 5;
  tennis->set_lane(150); tennis->set_players(2); tennis->reset(0);
  tennis->set_phase(games::GamePhase::Running);
  matrix.clear(); tail.clear();
  tennis->render(matrix,0); tennis->render_tail(tail,0);
  save_matrix((base+"/tail-tennis-0-matrix.ppm").c_str(), matrix);
  save_tail((base+"/tail-tennis-0-tail.ppm").c_str(), tail);

  PixelCanvas canvas;
  const uint32_t session=canvas.begin_session();
  std::string pixels(PixelCanvas::kHexChars,'0');
  pixels.replace(6u*299u,6,"0000ff");
  pixels.replace(6u*300u,6,"ffffff");
  pixels.replace(6u*449u,6,"00ff00");
  if (canvas.apply(session,1,pixels.c_str(),pixels.size(),false,1000)!=PixelCanvas::ApplyResult::Accepted) return 6;
  canvas.render(matrix,&tail);
  save_matrix((base+"/pixel-1000-matrix.ppm").c_str(), matrix);
  save_tail((base+"/pixel-1000-tail.ppm").c_str(), tail);
  return 0;
}
