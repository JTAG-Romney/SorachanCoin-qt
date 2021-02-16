// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2013-2015 The Novacoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <vector>
#include <inttypes.h>
#include <kernel.h>
#include <kernel_worker.h>
#include <util/thread.h>

void KernelWorker::Do_generic()
{
    bitthread::SetThreadPriority(THREAD_PRIORITY_LOWEST);

    // Compute maximum possible target to filter out majority of obviously insufficient hashes
    CBigNum bnTargetPerCoinDay; bnTargetPerCoinDay.SetCompact(nBits);
    // CScriptNum scTargetPerCoinDay;
    uint256 nMaxTarget = (bnTargetPerCoinDay * bnValueIn * block_check::nStakeMaxAge / util::COIN / util::nOneDay).getuint256();

    // Sha256 result buffer
    uint32_t hashProofOfStake[8];
    uint256 *pnHashProofOfStake = (uint256 *)&hashProofOfStake;

    // Search forward in time from the given timestamp
    // Stopping search in case of shutting down
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, kernel, 8 + 16);    // Init new sha256 context and update it with first 24 bytes of kernel
    SHA256_CTX workerCtx = ctx;                // save context
    for (uint32_t nTimeTx = nIntervalBegin, nMaxTarget32 = nMaxTarget.Get32(7); nTimeTx < nIntervalEnd && !args_bool::fShutdown; nTimeTx++)
    {
        // Complete first hashing iteration
        uint256 hash1;
        SHA256_Update(&ctx, (unsigned char *)&nTimeTx, 4);
        SHA256_Final((unsigned char *)&hash1, &ctx);
        SHA256((unsigned char *)&hash1, sizeof(hashProofOfStake), (unsigned char *)&hashProofOfStake); // Calculate kernel hash
        ctx = workerCtx;

        // Skip if hash doesn't satisfy the maximum target
        if (hashProofOfStake[7] > nMaxTarget32) {
            continue;
        } else {
            CBigNum bnCoinDayWeight = bnValueIn * bitkernel::GetWeight((int64_t)nInputTxTime, (int64_t)nTimeTx) / util::COIN / util::nOneDay;
            CBigNum bnTargetProofOfStake = bnCoinDayWeight * bnTargetPerCoinDay;
            if (bnTargetProofOfStake >= CBigNum(*pnHashProofOfStake)) {
                solutions.push_back(std::pair<uint256,uint32_t>(*pnHashProofOfStake, nTimeTx));
            }
        }
    }
}

//
// [static] Scan given kernel for solutions
//
bool KernelWorker::ScanKernelBackward(unsigned char *kernel, uint32_t nBits, uint32_t nInputTxTime, int64_t nValueIn, std::pair<uint32_t, uint32_t> &SearchInterval, std::pair<uint256, uint32_t> &solution)
{
    CBigNum bnTargetPerCoinDay; bnTargetPerCoinDay.SetCompact(nBits);
    CBigNum bnValueIn(nValueIn);

    // Get maximum possible target to filter out the majority of obviously insufficient hashes
    uint256 nMaxTarget = (bnTargetPerCoinDay * bnValueIn * block_check::nStakeMaxAge / util::COIN / util::nOneDay).getuint256();

    // Search backward in time from the given timestamp Stopping search in case of shutting down
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, kernel, 8 + 16);           // Init new sha256 context and update it with first 24 bytes of kernel
    SHA256_CTX workerCtx = ctx;                    // save context
    for (uint32_t nTimeTx = SearchInterval.first; nTimeTx > SearchInterval.second && !args_bool::fShutdown; nTimeTx--)
    {
        // Complete first hashing iteration
        uint256 hash1;
        SHA256_Update(&ctx, (unsigned char*)&nTimeTx, 4);
        SHA256_Final((unsigned char*)&hash1, &ctx);
        ctx = workerCtx;

        // Calculate kernel hash
        uint256 hashProofOfStake;
        SHA256((unsigned char*)&hash1, sizeof(hashProofOfStake), (unsigned char *)&hashProofOfStake);

        // Skip if hash doesn't satisfy the maximum target
        if (hashProofOfStake > nMaxTarget) {
            continue;
        } else {
            CBigNum bnCoinDayWeight = bnValueIn * bitkernel::GetWeight((int64_t)nInputTxTime, (int64_t)nTimeTx) / util::COIN / util::nOneDay;
            CBigNum bnTargetProofOfStake = bnCoinDayWeight * bnTargetPerCoinDay;
            if (bnTargetProofOfStake >= CBigNum(hashProofOfStake)) {
                solution.first = hashProofOfStake;
                solution.second = nTimeTx;
                return true;
            }
        }
    }
    return false;
}
