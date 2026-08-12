/**
 * Uses ReCpp the way a downstream project uses it: through the exported headers
 * and the exported static library, with no access to the ReCpp build tree.
 * It touches one symbol per exported subsystem, so a missing export fails the link.
 */
#include <rpp/strview.h>
#include <rpp/sprint.h>
#include <rpp/debugging.h>
#include <rpp/timer.h>
#include <rpp/paths.h>
#include <cstdio>  // printf
#include <string>  // std::string

int main()
{
    rpp::strview text = "consumer integration";
    if (text.next(' ') != "consumer")
    {
        printf("FAIL: strview::next\n");
        return 1;
    }

    std::string joined = rpp::concat(text, "!");
    if (joined != "integration!")
    {
        printf("FAIL: concat gave '%s'\n", joined.c_str());
        return 1;
    }

    rpp::Timer timer;
    if (timer.elapsed() < 0.0)
    {
        printf("FAIL: Timer::elapsed\n");
        return 1;
    }

    if (rpp::file_exists("this-file-does-not-exist"))
    {
        printf("FAIL: file_exists\n");
        return 1;
    }

    LogInfo("consumer linked ReCpp and ran %d checks", 4);
    printf("RppConsumer OK\n");
    return 0;
}
