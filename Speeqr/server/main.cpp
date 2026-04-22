#include "NtpServer.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/core/buffers_to_string.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <iomanip>

using namespace std;

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

static atomic<bool> g_shutdown{false};

// ── Read file ───────────────────────────────────────────────
static string readFile(const string& path) {
    ifstream f(path, ios::binary);
    if (!f) return "";
    return string(istreambuf_iterator<char>(f), {});
}

// ── Serve HTTP ──────────────────────────────────────────────
static void serveHttp(tcp::socket& sock,
                      http::request<http::string_body>& req)
{
    string target = string(req.target());
    if (target == "/" || target.empty()) target = "/index.html";

    string path = "../client" + target;
    string body = readFile(path);

    if (body.empty()) {
        http::response<http::string_body> res{
            http::status::not_found, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "404 Not Found: " + path;
        res.prepare_payload();
        beast::error_code ec;
        http::write(sock, res, ec);
        return;
    }

    string ct = "text/plain";
    if (target.size() >= 5 && target.substr(target.size() - 5) == ".html")
        ct = "text/html; charset=utf-8";
    else if (target.size() >= 3 && target.substr(target.size() - 3) == ".js")
        ct = "application/javascript; charset=utf-8";

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, ct);
    res.set(http::field::access_control_allow_origin, "*");
    res.body() = move(body);
    res.prepare_payload();

    beast::error_code ec;
    http::write(sock, res, ec);
}

// ── Handle WebSocket ────────────────────────────────────────
static void handleWebSocket(tcp::socket sock,
                            http::request<http::string_body> req)
{
    beast::error_code ec;
    beast::flat_buffer buf;

    websocket::stream<tcp::socket> ws(move(sock));
    ws.set_option(websocket::stream_base::timeout::suggested(
        beast::role_type::server));

    
    ws.accept(req, ec);
    if (ec) {
        cerr << "[WS] Accept error: " << ec.message() << "\n";
        return;
    }

    cout << "[WS] Client connected.\n";

    ws.text(true);
    ws.write(net::buffer(string(
        R"({"type":"welcome","msg":"Speeqr Day 1 ready"})")), ec);

    while (!g_shutdown) {
        buf.clear();
        ws.read(buf, ec);

        if (ec == websocket::error::closed) break;
        if (ec) {
            cerr << "[WS] Read error: " << ec.message() << "\n";
            break;
        }

        string msg = beast::buffers_to_string(buf.data());

        if (msg.find("ntp_sync") == string::npos) continue;

        // Parse T1
        double t1 = 0.0;
        auto pos = msg.find("\"t1\"");
        if (pos != string::npos) {
            pos = msg.find(':', pos);
            if (pos != string::npos) {
                t1 = stod(msg.substr(pos + 1));
            }
        }

        // T2 & T3
        double t2 = speeqr::NtpServer::currentNtpSeconds();
        double t3 = speeqr::NtpServer::currentNtpSeconds();

        // Build response
        ostringstream oss;
        oss << fixed << setprecision(6);
        oss << R"({"type":"ntp_response","t1":)" << t1
            << R"(,"t2":)" << t2
            << R"(,"t3":)" << t3 << "}";

        string response = oss.str();

        ws.text(true);
        ws.write(net::buffer(response), ec);

        if (ec) {
            cerr << "[WS] Write error: " << ec.message() << "\n";
            break;
        }

        cout << "[NTP] Sync: T2=" << t2 << " T3=" << t3 << "\n";
    }
}

// ── Handle TCP connection ───────────────────────────────────
static void handleConnection(tcp::socket sock) {
    beast::flat_buffer buf;
    beast::error_code  ec;

    http::request<http::string_body> req;
    http::read(sock, buf, req, ec);
    if (ec) return;

    if (websocket::is_upgrade(req)) {
        handleWebSocket(move(sock), move(req)); 
    } else {
        serveHttp(sock, req);
    }
}

// ── MAIN ────────────────────────────────────────────────────
int main() {
    const uint16_t wsPort  = 8080;
    const uint16_t ntpPort = 12300;

    cout << "\n=== Speeqr Day 1 Server ===\n";
    cout << "Web UI  : http://localhost:" << wsPort << "\n";
    cout << "NTP UDP : udp://localhost:" << ntpPort << "\n\n";

    net::io_context ioc;

    // Start NTP server
    speeqr::NtpServer ntpServer(ioc, ntpPort);
    ntpServer.start();

    // TCP acceptor
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), wsPort));
    acceptor.set_option(net::socket_base::reuse_address(true));

    cout << "[Main] Waiting for connections...\n";

    thread ioThread([&ioc]() { ioc.run(); });

    while (!g_shutdown) {
        boost::system::error_code ec;
        tcp::socket sock(ioc);
        acceptor.accept(sock, ec);

        if (ec) {
            cerr << "[Main] Accept error: " << ec.message() << "\n";
            continue;
        }

        thread([s = move(sock)]() mutable {
            handleConnection(move(s));
        }).detach();
    }

    ioc.stop();
    ioThread.join();

    cout << "[Main] Server stopped.\n";
    return 0;
}