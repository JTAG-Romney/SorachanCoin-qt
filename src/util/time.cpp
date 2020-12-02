// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//#if defined(HAVE_CONFIG_H)
//# include <config/bitcoin-config.h>
//#endif

#include <util/time.h>
#include <atomic>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <ctime>
#include <util/tinyformat.h>

namespace util {
    static std::atomic<int64_t> nMockTime(0); //!< For unit testing
}

int64_t util::GetTime() noexcept {
    int64_t mocktime = nMockTime.load(std::memory_order_relaxed);
    if (mocktime) return mocktime;

    time_t now = ::time(nullptr);
    assert(now > 0);
    return now;
}

void util::SetMockTime(int64_t nMockTimeIn) noexcept {
    nMockTime.store(nMockTimeIn, std::memory_order_relaxed);
}

int64_t util::GetMockTime() noexcept {
    return nMockTime.load(std::memory_order_relaxed);
}

int64_t util::GetTimeMillis() noexcept {
    int64_t now = (boost::posix_time::microsec_clock::universal_time() -
                   boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
    assert(now > 0);
    return now;
}

int64_t util::GetTimeMicros() noexcept {
    int64_t now = (boost::posix_time::microsec_clock::universal_time() -
                   boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
    assert(now > 0);
    return now;
}

int64_t util::GetSystemTimeInSeconds() noexcept {
    return GetTimeMicros()/1000000;
}

void util::MilliSleep(int64_t n) noexcept {
#if BOOST_VERSION >= 105000
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
#else
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
#endif
/**
 * Boost's sleep_for was uninterruptible when backed by nanosleep from 1.50
 * until fixed in 1.52. Use the deprecated sleep method for the broken case.
 * See: https://svn.boost.org/trac/boost/ticket/7238
 */
//#if defined(HAVE_WORKING_BOOST_SLEEP_FOR)
//    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
//#elif defined(HAVE_WORKING_BOOST_SLEEP)
//    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
//#else
//should never get here
//#error missing boost sleep implementation
//#endif
}

std::string util::FormatISO8601DateTime(int64_t nTime) noexcept {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _MSC_VER
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%04i-%02i-%02iT%02i:%02i:%02iZ", ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);
}

std::string util::FormatISO8601Date(int64_t nTime) noexcept {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _MSC_VER
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%04i-%02i-%02i", ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday);
}

std::string util::FormatISO8601Time(int64_t nTime) noexcept {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _MSC_VER
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%02i:%02i:%02iZ", ts.tm_hour, ts.tm_min, ts.tm_sec);
}
