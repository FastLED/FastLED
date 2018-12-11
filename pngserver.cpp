#ifdef __unix__

#define FASTLED_INTERNAL
#include <set>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "FastLED.h"
#include "pngserver.hpp"

FASTLED_NAMESPACE_BEGIN

// WebSocket PNG server using WebSocket++ library, based on example code
// https://github.com/zaphoyd/websocketpp/tree/master/examples/broadcast_server
//
// Serves a tiny HTTP page which has JavaScript to listen for a stream of
// PNG files over a WebSocket and display them. Pushes each frame over the WebSocket.

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;

PngServer::PngServer(uint16_t port)
    : m_port(port), m_server_started(false) {
  // Set up WebSocket + HTTP server.
  m_server.clear_access_channels(websocketpp::log::alevel::all);
  m_server.init_asio();
  m_server.set_reuse_addr(true);
  m_server.set_open_handler(bind(&PngServer::on_open, this, ::_1));
  m_server.set_close_handler(bind(&PngServer::on_close, this, ::_1));
  m_server.set_http_handler(bind(&PngServer::on_http, this, _1));
  spawn_thread();
}

void PngServer::register_image(uint8_t image_index) {
  if (image_index >= m_num_images) {
    m_num_images = image_index + 1;
  }
}

void PngServer::spawn_thread() {
  if (!m_server_started) {
    m_server_started = true;
    if (int rc = pthread_create(&m_server_thread, NULL, &PngServer::run_server,
                                (void *)this)) {
      fprintf(stderr, "Error %d creating thread\n", rc);
      exit(1);
    }
  }
}

void PngServer::send_image(uint8_t imageIndex, size_t width,
                           unsigned char *buf) {
  // Construct a binary WebSocket message, with the following format:
  std::string str;
  // First send the imageIndex (as one byte).
  str.push_back(imageIndex);
  // Then send the PNG immediately after.
  Png png(width, buf);
  str.append(png.data(), png.len());

  // Now send it to all listening clients.
  for (auto it : m_connections) {
    m_server.send(it, str, websocketpp::frame::opcode::BINARY);
  }
}

void PngServer::run() {
  // Start server running, listening on the indicated port.
  m_server.listen(m_port);
  m_server.start_accept();
  m_server.run();
}

void *PngServer::run_server(void *arg) {
  PngServer *server = (PngServer *)arg;
  server->run();
}

void PngServer::on_open(connection_hdl hdl) {
  // Track WebSocket connections.
  m_connections.insert(hdl);
}

void PngServer::on_close(connection_hdl hdl) {
  // Track WebSocket connections.
  m_connections.erase(hdl);
}

void PngServer::on_http(connection_hdl hdl) {
  // Respond to HTTP request (at any path) with HTML/JavaScript code to
  // retrieve the data from the websocket and render it nicely.
  //
  // Supports `m_num_images` images.
  server::connection_ptr con = m_server.get_con_from_hdl(hdl);
  std::string response = "<!DOCTYPE html>\n\
<html>\n\
<head>\n\
<style type=\"text/css\">\n\
body { background-color: #000;  }\n\
img { width: 50%; margin: 50px auto; display: block; }\n\
</style>\n\
</head>\n\
<body>\n";
  for (int i = 0; i < m_num_images; i++) {
    response.append("<img id=\"image" + std::to_string(i) + "\"/>\n");
  }
  response.append("<script>\n\
  var image = [];\n");
  for (int i = 0; i < m_num_images; i++) {
    response.append("image[" + std::to_string(i) +
                    "] = document.getElementById(\"image" + std::to_string(i) +
                    "\");\n");
  }
  // Write PNG `i` to image `i % length` to avoid ever going out of bounds.
  response.append(
      "var conn = new WebSocket(\"ws://\" + location.host + \"/\");\n\
  conn.onmessage = function(e) {\n\
    var url = URL.createObjectURL(e.data.slice(1, undefined, \"image/png\"));\n\
    var fileReader = new FileReader();\n\
    fileReader.onload = function(evt) {\n\
      var imageIndex = new Uint8Array(evt.target.result)[0] % image.length;\n\
      image[imageIndex].src = url;\n\
    }\n\
    fileReader.readAsArrayBuffer(e.data.slice(0, 1));\n\
  };\n\
</script>\n\
</body>\n\
</html>");
  con->replace_header("Content-Type", "text/html; charset=utf-8");
  con->set_body(response);
  con->set_status(websocketpp::http::status_code::ok);
}

uint8_t PNGSERVER_SRGB_FROM_LINEAR[] = {
    // Numbers calculated from formula in https://en.wikipedia.org/wiki/SRGB
    // i.e.,  (v<=0.0031308) ? 12.92*v : 1.055*POW(v, 1/2.4)-0.055.
    0,   13,  22,  28,  34,  38,  42,  46,  50,  53,  56,  59,  61,  64,  66,
    69,  71,  73,  75,  77,  79,  81,  83,  85,  86,  88,  90,  92,  93,  95,
    96,  98,  99,  101, 102, 104, 105, 106, 108, 109, 110, 112, 113, 114, 115,
    117, 118, 119, 120, 121, 122, 124, 125, 126, 127, 128, 129, 130, 131, 132,
    133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147,
    148, 148, 149, 150, 151, 152, 153, 154, 155, 155, 156, 157, 158, 159, 159,
    160, 161, 162, 163, 163, 164, 165, 166, 167, 167, 168, 169, 170, 170, 171,
    172, 173, 173, 174, 175, 175, 176, 177, 178, 178, 179, 180, 180, 181, 182,
    182, 183, 184, 185, 185, 186, 187, 187, 188, 189, 189, 190, 190, 191, 192,
    192, 193, 194, 194, 195, 196, 196, 197, 197, 198, 199, 199, 200, 200, 201,
    202, 202, 203, 203, 204, 205, 205, 206, 206, 207, 208, 208, 209, 209, 210,
    210, 211, 212, 212, 213, 213, 214, 214, 215, 215, 216, 216, 217, 218, 218,
    219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224, 224, 225, 226, 226,
    227, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232, 233, 233, 234,
    234, 235, 235, 236, 236, 237, 237, 238, 238, 238, 239, 239, 240, 240, 241,
    241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 247, 247, 248,
    248, 249, 249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 254, 254, 255,
    255};

FASTLED_NAMESPACE_END

#endif
