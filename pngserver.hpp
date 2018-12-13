#ifndef __INC_PNGSERVER_HPP
#define __INC_PNGSERVER_HPP
#include <set>
#include <map>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "FastLED.h"
#include "png.hpp"

FASTLED_NAMESPACE_BEGIN

// WebSocket PNG server using WebSocket++ library, based on example code
// https://github.com/zaphoyd/websocketpp/tree/master/examples/broadcast_server
//
// Serves a tiny HTTP page which has JavaScript to listen for a stream of
// PNG files over a WebSocket and display them. Pushes each frame over the WebSocket.

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;

extern uint8_t PNGSERVER_SRGB_FROM_LINEAR[];

/// Serves PNG files over WebSockets.
class PngServer {
public:
  /// Create a new PNG server. Starts with 0 images configured;
  /// you must register each image with `register_image`.
  PngServer(uint16_t port);

  /// Start the server running in a separate thread.
  /// Idempotent.
  void spawn_thread();

  /// Register an image index which the PNG server may be asked to serve.
  void register_image(uint8_t image_index);

  /// Send image `imageIndex`, made up of `width` pixels, each of which is
  /// described by three bytes (R, G, B), starting at `buf`.
  /// The default (sRGB) colour space is used.
  void send_image(uint8_t imageIndex, size_t width, unsigned char *buf);

  // Convert a byte from linear to sRGB. Utility function.
  static inline uint8_t sRGB_from_linear(uint8_t value) {
    return PNGSERVER_SRGB_FROM_LINEAR[value];
  }

protected:
  static void *run_server(void *arg);

  void run();

  void on_open(connection_hdl hdl);

  void on_close(connection_hdl hdl);

  void on_http(connection_hdl hdl);

  PngServer(const PngServer&) = delete;

private:
  typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;

  /// Port to listen on.
  const uint16_t m_port;

  /// Number of images to render.
  uint8_t m_num_images;

  /// Underlying WebSocket++ server.
  server m_server;

  /// Server thread.
  pthread_t m_server_thread;

  /// Has server thread been started?
  bool m_server_started;

  /// List of connections.
  con_list m_connections;

  /// PNG servers, per port.
  static std::map<uint16_t, PngServer> m_png_servers;
};

FASTLED_NAMESPACE_END

#endif
