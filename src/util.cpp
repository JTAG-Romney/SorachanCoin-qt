// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2018-2021 The SorachanCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util.h>
#include <sync/sync.h>
#include <version.h>
#include <ui_interface.h>
#include <boost/algorithm/string/join.hpp>
#include <thread/threadsafety.h>
#include <util/strencodings.h>
#include <util/exception.h>

// Work around clang compilation problem in Boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function 'to_internal' that is neither visible in the template definition nor found by argument-dependent lookup
// See also: http://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           http://clang.debian.net/status.php?version=3.0&key=CANNOT_FIND_FUNCTION
/*
namespace boost {
    namespace program_options {
        std::string to_internal(const std::string &);
    }
}
*/

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <boost/thread.hpp>
#include <cleanse/cleanse.h>
#include <openssl/crypto.h>

#if !defined(WIN32) && !defined(ANDROID) && !defined(__OpenBSD__)
# include <execinfo.h>
#endif

//FILE *trace::_fileout = nullptr;
int64_t bitsystem::nNodesOffset = INT64_MAX;
// Extended dump::DecodeDumpTime implementation, see this page for details:
// http://stackoverflow.com/questions/3786201/parsing-of-date-time-from-string-boost
const std::locale dump::formats[5] = {
    std::locale(std::locale::classic(),new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ")),
    std::locale(std::locale::classic(),new boost::posix_time::time_input_facet("%Y-%m-%d %H:%M:%S")),
    std::locale(std::locale::classic(),new boost::posix_time::time_input_facet("%Y/%m/%d %H:%M:%S")),
    std::locale(std::locale::classic(),new boost::posix_time::time_input_facet("%d.%m.%Y %H:%M:%S")),
    std::locale(std::locale::classic(),new boost::posix_time::time_input_facet("%Y-%m-%d"))
};
LockedPageManager LockedPageManager::instance;    // allocators.h (Singleton class)
CMedianFilter<int64_t> bitsystem::vTimeOffsets(200,0);

void seed::RandAddSeed()
{
    auto GetPerformanceCounter = []() {
        int64_t nCounter = 0;
#ifdef WIN32
        ::QueryPerformanceCounter((LARGE_INTEGER *)&nCounter);
#else
        timeval t;
        ::gettimeofday(&t, nullptr);
        nCounter = (int64_t)t.tv_sec * 1000000 + t.tv_usec;
#endif
        return nCounter;
    };

    // Seed with CPU performance counter
    int64_t nCounter = GetPerformanceCounter();
    ::RAND_add(&nCounter, sizeof(nCounter), 1.5);
    cleanse::OPENSSL_cleanse(&nCounter, sizeof(nCounter));
}

void seed::RandAddSeedPerfmon()
{
    seed::RandAddSeed();

    // This can take up to 2 seconds, so only do it every 10 minutes
    static int64_t nLastPerfmon = 0;
    if (bitsystem::GetTime() < nLastPerfmon + 10 * 60) {
        return;
    }

    nLastPerfmon = bitsystem::GetTime();

#ifdef WIN32
    // Don't need this on Linux, OpenSSL automatically uses /dev/urandom
    // Seed with the entire set of perfmon data
    unsigned char pdata[250000] = { 0 };
    unsigned long nSize = sizeof(pdata);
    long ret = ::RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", nullptr, nullptr, pdata, &nSize);
    ::RegCloseKey(HKEY_PERFORMANCE_DATA);
    if (ret == ERROR_SUCCESS) {
        ::RAND_add(pdata, nSize, nSize / 100.0);
        cleanse::OPENSSL_cleanse(pdata, nSize);
        logging::LogPrintf("seed::RandAddSeed() %lu bytes\n", nSize);
    }
#endif
}

int64_t dump::DecodeDumpTime(const std::string &s)
{
    boost::posix_time::ptime pt;

    size_t formats_n = sizeof(dump::formats) / sizeof(dump::formats[0]);
    for(size_t i=0; i < formats_n; ++i)
    {
        std::istringstream is(s);
        is.imbue(dump::formats[i]);
        is >> pt;
        if(pt != boost::posix_time::ptime()) { break; }
    }

    return dump::pt_to_time_t(pt);
}

std::string dump::EncodeDumpTime(int64_t nTime) {
    return util::DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

std::string dump::EncodeDumpString(const std::string &str) {
    std::stringstream ret;

    for(unsigned char c: str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << util::HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string dump::DecodeDumpString(const std::string &str)
{
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); ++pos)
    {
        unsigned char c = str[pos];
        if (c == '%' && pos + 2 < str.length()) {
            c = (((str[pos+1] >> 6) * 9 + ((str[pos+1] - '0')&15)) << 4) |
                ((str[pos+2] >> 6) * 9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

void bitsystem::AddTimeData(const CNetAddr &ip, int64_t nTime)
{
    int64_t nOffsetSample = nTime - bitsystem::GetTime();

    // Ignore duplicates
    static std::set<CNetAddr> setKnown;
    if (! setKnown.insert(ip).second) {
        return;
    }

    // Add data
    vTimeOffsets.input(nOffsetSample);
    logging::LogPrintf("Added time data, samples %d, offset %+" PRId64 " (%+" PRId64 " minutes)\n", vTimeOffsets.size(), nOffsetSample, nOffsetSample / 60);
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1) {
        int64_t nMedian = vTimeOffsets.median();
        std::vector<int64_t> vSorted = vTimeOffsets.sorted();

        // Only let other nodes change our time by so much
        if (util::abs64(nMedian) < 70 * 60) {
            nNodesOffset = nMedian;
        } else {
            nNodesOffset = INT64_MAX;

            static bool fDone;
            if (! fDone) {
                bool fMatch = false;

                // If nobody has a time different than ours but within 5 minutes of ours, give a warning
                for(int64_t nOffset: vSorted) {
                    if (nOffset != 0 && util::abs64(nOffset) < 5 * 60) {
                        fMatch = true;
                    }
                }

                if (! fMatch) {
                    fDone = true;
                    std::string strMessage = _("Warning: Please check that your computer's date and time are correct! If your clock is wrong " strCoinName " will not work properly.");
                    excep::set_strMiscWarning( strMessage );
                    logging::LogPrintf("*** %s\n", strMessage.c_str());
                    CClientUIInterface::uiInterface.ThreadSafeMessageBox(strMessage+" ", strCoinName, CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION);
                }
            }
        }
        if (args_bool::fDebug) {
            for(int64_t n: vSorted) {
                logging::LogPrintf("%+" PRId64 "  ", n);
            }
            logging::LogPrintf("|  ");
        }
        if (nNodesOffset != INT64_MAX) {
            logging::LogPrintf("nNodesOffset = %+" PRId64 "  (%+" PRId64 " minutes)\n", nNodesOffset, nNodesOffset / 60);
        }
    }
}

/*
void cmd::runCommand(std::string strCommand)
{
    int nErr = ::system(strCommand.c_str());
    if (nErr) {
        logging::LogPrintf("cmd::runCommand error: system(%s) returned %d\n", strCommand.c_str(), nErr);
    }
}
*/
