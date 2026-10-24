// NO100F holiday option ([game] holiday, launcher "Holidays"). The hubs' holiday events (fn_800AED5C, run when H001 or
// H002 loads) ask two tiny getters for today's date: fn_80016644 returns the month (1-12) and fn_80016674 the day of the
// month, both from OSGetTime + OSTicksToCalendarTime. Nothing else calls them, so overriding them picks the holiday
// without changing the date the rest of the game sees. "auto" answers the PC's local date.
//
// Dates the game reacts to: Jan 1, Feb 2, Mar 22, May 30, Aug 28, Sep 8, Nov 5, Dec 20, Dec 31 fireworks; Mar 17 green
// chimney smoke (the "FOUNTAIN EMITTER" objects); Jul 4 red/white/blue smoke + fireworks; Oct 31 bats over the manor
// door (BAT HANG 0-19); Dec 14 and Dec 25 snow (other December days set the snow object up without visible snow).
// "off" answers June 15 (June has no events).
#include <ctime>
#include <string>
#include "hle_stubs.h"
#include "ppc_runtime.h"
#include "runtime_config.h"

namespace {
struct MonthDay { uint32_t month, day; };

MonthDay HolidayDate() {
    static const std::string mode = RuntimeConfigFile::Holiday();
    if (mode == "off") return {6, 15};
    if (mode != "auto")  // "MM-DD", validated when the config is read
        return {static_cast<uint32_t>((mode[0] - '0') * 10 + (mode[1] - '0')),
                static_cast<uint32_t>((mode[3] - '0') * 10 + (mode[4] - '0'))};
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {static_cast<uint32_t>(local.tm_mon + 1), static_cast<uint32_t>(local.tm_mday)};
}
}  // namespace

extern "C" void No100f_HolidayMonth(CpuContext* ctx) { ctx->gpr[3] = HolidayDate().month; }
extern "C" void No100f_HolidayDay(CpuContext* ctx) { ctx->gpr[3] = HolidayDate().day; }
PPC_NATIVE_OVERRIDE_VOID(80016644, No100f_HolidayMonth, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(80016674, No100f_HolidayDay, (CpuContext* ctx), (ctx));
