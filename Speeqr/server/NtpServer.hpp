#pragma once

#ifndef SPEEQR_NTP_SERVER_HPP
#define SPEEQR_NTP_SERVER_HPP

#include <boost/asio.hpp>
#include <array>
#include <cstdint>
#include <atomic>
#include <chrono>

namespace speeqr {

// NTP epoch offset (1900 → 1970)
constexpr uint64_t kNtpUnixDelta = 2208988800ULL;

// Fraction scale (2^32)
constexpr double kNtpFracScale = 4294967296.0;

// ── NTP Packet (48 bytes) ───────────────────────────────────
#pragma pack(push, 1)
struct NtpPacket {
    uint8_t  li_vn_mode   = 0;
    uint8_t  stratum      = 1;
    uint8_t  poll         = 4;
    int8_t   precision    = -20;
    uint32_t rootDelay      = 0;
    uint32_t rootDispersion = 0;
    uint32_t referenceId    = 0;
    uint32_t refTsSec  = 0;  uint32_t refTsFrac  = 0;
    uint32_t origTsSec = 0;  uint32_t origTsFrac = 0;
    uint32_t rxTsSec   = 0;  uint32_t rxTsFrac   = 0;
    uint32_t txTsSec   = 0;  uint32_t txTsFrac   = 0;
};
#pragma pack(pop)

static_assert(sizeof(NtpPacket) == 48, "NtpPacket must be 48 bytes");

// ── NtpServer ───────────────────────────────────────────────
class NtpServer {
public:
    explicit NtpServer(boost::asio::io_context& io, uint16_t port = 12300);

    NtpServer(const NtpServer&)            = delete;
    NtpServer& operator=(const NtpServer&) = delete;
    ~NtpServer();

    static double currentNtpSeconds();

    void start();
    void stop();

private:
    void doReceive();

    void sendResponse(const NtpPacket& req,
                      const boost::asio::ip::udp::endpoint& sender,
                      uint32_t rxSec,
                      uint32_t rxFrac,
                      double rxTime);   

    static void splitTime(double t, uint32_t& sec, uint32_t& frac);
    static uint32_t toBE(uint32_t v);

    boost::asio::ip::udp::socket   socket_;
    boost::asio::ip::udp::endpoint senderEndpoint_;
    std::array<uint8_t, 48>        recvBuf_{};
    std::atomic<bool>              running_{false};

    double anchorNtpSec_{0.0};
    std::chrono::steady_clock::time_point anchorPoint_;
};

} // namespace speeqr

#endif // SPEEQR_NTP_SERVER_HPP