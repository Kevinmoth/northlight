#pragma once
#include <new>
#include <exception>
namespace NorthlightExtensionGuard {
enum class Fault { None, Allocation, Exception, Unknown };
// Boundary around extension work only. The original game API call belongs
// outside this boundary and must still execute after a failed extension.
template<class Work, class Report>
bool run(Work&& work, Report&& report) noexcept {
    try { work(); return true; }
    catch (const std::bad_alloc&) { report(Fault::Allocation); }
    catch (const std::exception&) { report(Fault::Exception); }
    catch (...) { report(Fault::Unknown); }
    return false;
}
}
