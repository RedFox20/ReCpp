#pragma once
#include <rpp/debugging.h> // rpp::add_log_handler, rpp::remove_log_handler
#include <rpp/semaphore.h>
#include <rpp/timepoint.h> // rpp::seconds
#include <string>

/// Captures a warning which contains `marker`, from its constructor until wait() returns
struct warning_capture
{
    const char* marker;
    rpp::semaphore logged;
    std::string text;

    explicit warning_capture(const char* marker) noexcept : marker{marker} { rpp::add_log_handler(this, &capture); }
    ~warning_capture() noexcept { rpp::remove_log_handler(this, &capture); }
    warning_capture(const warning_capture&) = delete;
    warning_capture& operator=(const warning_capture&) = delete;

    /// @returns the warning, or an empty string when no warning arrives
    std::string wait()
    {
        (void)logged.wait(rpp::seconds(1)); // a hang guard, the warning releases it
        rpp::remove_log_handler(this, &capture); // waits for a running handler, so `text` is safe to read
        return text;
    }

private:
    static void capture(void* context, LogSeverity severity, const char* message, int len)
    {
        auto* self = static_cast<warning_capture*>(context);
        std::string warning { message, size_t(len) };
        if (severity != LogSeverityWarn || warning.find(self->marker) == std::string::npos) return;
        self->text = std::move(warning);
        self->logged.notify();
    }
};
