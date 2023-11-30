#include "load_balancer.h"

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////

    void load_balancer::set_max_bytes_per_sec(uint32_t maxBytesPerSec) noexcept
    {
        this->maxBytesPerSec = maxBytesPerSec;

        // the load balancer is disabled when set to rate=0
        if (maxBytesPerSec == 0)
        {
            nanosBetweenBytes = 0;
            return;
        }

        nanosBetweenBytes = 1'000'000'000 / maxBytesPerSec;
        // need at least 1ns between bytes
        // for a 280 byte UDP packet, we would have to wait 280ns between packets
        // this would give us a max of 3.5 million packets per second
        // witch sleep inefficiencies closer to 1 million packets per second
        if (nanosBetweenBytes == 0)
            nanosBetweenBytes = 1;
    }

    bool load_balancer::can_send() const noexcept
    {
        return can_send(TimePoint::now());
    }

    // DON'T FORGET TO CALL notify_sent() !
    bool load_balancer::can_send(const rpp::TimePoint& now) const noexcept
    {
        if (!lastSendTime)
            return true;

        const int64_t waitTimeNs = int64_t(nextSendTimeout) - lastSendTime.elapsed_ns(now);
        return waitTimeNs <= 0;
    }

    void load_balancer::wait_to_send(uint32_t bytesToSend) noexcept
    {
        TimePoint start = TimePoint::now();
        const rpp::TimePoint timeoutStart = lastSendTime;
        if (!timeoutStart)
        {
            notify_sent(start, bytesToSend); // first time (no wait)
            return;
        }

        TimePoint end = start;
        const int64_t timeoutNs = int64_t(nextSendTimeout);
        const int64_t waitTimeNs = timeoutNs - timeoutStart.elapsed_ns(start);
        if (waitTimeNs > 0)
        {
            int64_t remainingNs = waitTimeNs;

            // UDP socket sendto also takes a small amount of time
            // so we quit if we have minRemainingNs left
            constexpr int64_t minRemainingNs = 80;
            while (remainingNs > minRemainingNs)
            {
                // if we have very little time remaining
                // do a quick yield instead of sleeping
                if (remainingNs < 150'000)
                {
                    std::this_thread::yield();
                }
                else
                {
                    rpp::sleep_ns(remainingNs / 2);
                }

                end = TimePoint::now();
                remainingNs = timeoutNs - timeoutStart.elapsed_ns(end);
            }
        }

        notify_sent(end, bytesToSend);
    }

    void load_balancer::notify_sent(const rpp::TimePoint& now, uint32_t bytesToSend) noexcept
    {
        lastSendTime = now;
        nextSendTimeout = bytesToSend * nanosBetweenBytes;
    }

    ////////////////////////////////////////////////////////////////////////////////
}
