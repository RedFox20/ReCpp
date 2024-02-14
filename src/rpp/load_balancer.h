#pragma once
#include "config.h"
#include "timer.h" // rpp::TimePoint

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * Helper utility for UDP sockets to perform load balancing on send operations
     */
    class RPPAPI load_balancer
    {
        uint32 maxBytesPerSec;
        uint32 nanosBetweenBytes;
        rpp::TimePoint lastSendTime;
        int32 nextSendTimeout = 0;

    public:
        explicit load_balancer(uint32 maxBytesPerSec) noexcept
        {
            set_max_bytes_per_sec(maxBytesPerSec);
        }

        /** @returns Current bytes per sec limit */
        uint32 get_max_bytes_per_sec() const noexcept { return maxBytesPerSec; }

        /** @brief Sets the new max bytes per second limit */
        void set_max_bytes_per_sec(uint32 maxBytesPerSec) noexcept;

        /** @returns Average nanoseconds between bytes */
        uint32 avg_nanos_between_bytes() const noexcept { return nanosBetweenBytes; }

        /**
         * @returns TRUE if the specified number of bytes can be sent
         * @warning The caller MUST MANUALLY call notify_sent() after sending the bytes
         */
        bool can_send() const noexcept;
        bool can_send(const rpp::TimePoint& now) const noexcept;

        /**
         * @brief Blocks until the specified number of bytes can be sent
         *        and notifies the load balancer that we sent the bytes
         */
        void wait_to_send(uint32 bytesToSend) noexcept;

        /**
         * @brief Notify that we sent this # of bytes
         */
        void notify_sent(const rpp::TimePoint& now, uint32 bytesToSend) noexcept;
    };

    ////////////////////////////////////////////////////////////////////////////////

}
