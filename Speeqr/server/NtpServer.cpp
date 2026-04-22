#include "NtpServer.hpp"

#include <chrono>
#include <cstring>
#include <iostream>

using namespace std;

namespace speeqr {

// ── Convert uint32 → big-endian ─────────────────────────────
uint32_t NtpServer::toBE(uint32_t v) {
#if defined(_MSC_VER)
    return _byteswap_ulong(v);
#else
    return __builtin_bswap32(v);
#endif
}

// ── Split time into seconds + fraction ──────────────────────
void NtpServer::splitTime(double t, uint32_t& sec, uint32_t& frac) {
    sec  = static_cast<uint32_t>(t);
    frac = static_cast<uint32_t>((t - sec) * kNtpFracScale);
}

// ── Constructor ─────────────────────────────────────────────
NtpServer::NtpServer(boost::asio::io_context& io, uint16_t port)
    : socket_(io, boost::asio::ip::udp::endpoint(
                      boost::asio::ip::udp::v4(), port))
{
    anchorPoint_ = chrono::steady_clock::now();

    auto sysNow = chrono::system_clock::now();
    double sysEpoch = chrono::duration<double>(
        sysNow.time_since_epoch()).count();

    anchorNtpSec_ = sysEpoch + static_cast<double>(kNtpUnixDelta);

    cout << "[NTP] Listening on UDP port " << port << "\n";
}

// ── Destructor ──────────────────────────────────────────────
NtpServer::~NtpServer() {
    stop();
}

// ── Current NTP time (high precision) ───────────────────────
double NtpServer::currentNtpSeconds() {
    static const auto s_anchor = chrono::steady_clock::now();

    static const double s_anchorNtp = []() {
        auto sys = chrono::system_clock::now();
        return chrono::duration<double>(sys.time_since_epoch()).count()
               + static_cast<double>(kNtpUnixDelta);
    }();

    auto elapsed = chrono::steady_clock::now() - s_anchor;
    return s_anchorNtp + chrono::duration<double>(elapsed).count();
}

// ── Start server ────────────────────────────────────────────
void NtpServer::start() {
    running_ = true;
    doReceive();
    cout << "[NTP] Ready.\n";
}

// ── Stop server ─────────────────────────────────────────────
void NtpServer::stop() {
    running_ = false;
    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.close(ec);
}

// ── Receive packets ─────────────────────────────────────────
void NtpServer::doReceive() {
    socket_.async_receive_from(
        boost::asio::buffer(recvBuf_),
        senderEndpoint_,
        [this](boost::system::error_code ec, size_t bytes)
        {
            if (ec) {
                if (running_)
                    cerr << "[NTP] Receive error: " << ec.message() << "\n";
                return;
            }

            // T2 (receive time)
            double rxTime = currentNtpSeconds();

            uint32_t rxSec, rxFrac;
            splitTime(rxTime, rxSec, rxFrac);

            if (bytes >= 48) {
                NtpPacket req{};
                memcpy(&req, recvBuf_.data(), 48);

                sendResponse(req, senderEndpoint_, rxSec, rxFrac, rxTime);
            }

            if (running_) doReceive();
        }
    );
}

// ── Send response ───────────────────────────────────────────
void NtpServer::sendResponse(const NtpPacket& req,
                            const boost::asio::ip::udp::endpoint& sender,
                            uint32_t rxSec, uint32_t rxFrac,
                            double rxTime)
{
    NtpPacket resp{};

    resp.li_vn_mode     = 0x24;
    resp.stratum        = 1;
    resp.poll           = 4;
    resp.precision      = -20;
    resp.rootDelay      = 0;
    resp.rootDispersion = 0;
    resp.referenceId    = toBE(0x4C4F434C); // "LOCL"

    // Reference timestamp
    uint32_t refSec, refFrac;
    splitTime(anchorNtpSec_, refSec, refFrac);
    resp.refTsSec  = toBE(refSec);
    resp.refTsFrac = toBE(refFrac);

    // Originate (T1)
    resp.origTsSec  = req.txTsSec;
    resp.origTsFrac = req.txTsFrac;

    // Receive (T2)
    resp.rxTsSec  = toBE(rxSec);
    resp.rxTsFrac = toBE(rxFrac);

    // Transmit (T3)
    double txTime = currentNtpSeconds();

    uint32_t txSec, txFrac;
    splitTime(txTime, txSec, txFrac);

    resp.txTsSec  = toBE(txSec);
    resp.txTsFrac = toBE(txFrac);

    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(&resp, sizeof(resp)), sender, 0, ec);

    if (!ec) {
        cout << "[NTP] Responded to "
             << sender.address().to_string()
             << " | processing ~ "
             << ((txTime - rxTime) * 1000.0) << " ms\n";
    }
}

} // namespace speeqr