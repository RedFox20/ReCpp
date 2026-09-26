#pragma once
#include <rpp/debugging.h> // rpp::add_log_handler, rpp::remove_log_handler
#include <rpp/semaphore.h>
#include <rpp/strview.h> // rpp::strview
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

    /// @returns true when the warning arrives and also contains `part`
    bool wait(const char* part)
    {
        (void)logged.wait(rpp::seconds(1)); // a hang guard, the warning releases it
        rpp::remove_log_handler(this, &capture); // waits for a running handler, so `text` is safe to read
        return rpp::strview{text}.contains(part);
    }

private:
    static void capture(void* context, LogSeverity severity, const char* message, int len)
    {
        auto* self = static_cast<warning_capture*>(context);
        if (severity != LogSeverityWarn || !rpp::strview{message, len}.contains(self->marker)) return;
        self->text.assign(message, size_t(len));
        self->logged.notify();
    }
};
