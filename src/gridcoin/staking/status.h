// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "sync.h"
#include "uint256.h"

#include <atomic>
#include <optional>
#include <string>

class CWallet;
class CWalletTx;

namespace GRC {
//!
//! \brief Stores staking information generated by the miner thread for GUI/RPC
//! reporting.
//!
//! THREAD SAFETY: this API is thread-safe.
//!
class MinerStatus
{
public:
    //!
    //! \brief Represents problems that prevent staking.
    //!
    //! Update MinerStatus::FormatErrors() when adding or removing items.
    //!
    enum ErrorFlags
    {
        NONE                               = 0,
        DISABLED_BY_CONFIGURATION          = (1 << 0),
        NO_MATURE_COINS                    = (1 << 1),
        NO_COINS                           = (1 << 2),
        ENTIRE_BALANCE_RESERVED            = (1 << 3),
        NO_UTXOS_AVAILABLE_DUE_TO_RESERVE  = (1 << 4),
        WALLET_LOCKED                      = (1 << 5),
        TESTNET_ONLY                       = (1 << 6),
        OFFLINE                            = (1 << 7),
        OUT_OF_BOUND, //!< Enumeration end-of-range marker value.
    };

    //!
    //! \brief Contains the miner status information related to the last kernel
    //! search cycle performed by the miner thread.
    //!
    class SearchReport
    {
    public:
        int m_block_version = 0;
        int64_t m_timestamp = 0;

        uint64_t m_weight_sum = 0;
        uint64_t m_weight_min = 0;
        uint64_t m_weight_max = 0;
        double m_value_sum = 0;

        uint64_t m_blocks_created = 0;
        uint64_t m_blocks_accepted = 0;
        uint64_t m_kernels_found = 0;

        //!
        //! \brief Calculate the total coin weight of the UTXOs searched in the
        //! last miner cycle.
        //!
        //! Note: since block version 8, this is approximately the same as the
        //! total GRC value of the staked UTXOs.
        //!
        double CoinWeight() const;

        //!
        //! \brief Get the number of discovered kernel proofs in blocks rejected
        //! by the consensus validation since the node started.
        //!
        size_t KernelsRejected() const;
    }; // SearchReport

    //!
    //! \brief Contains the miner status information needed to calculate the
    //! efficiency of the miner thread.
    //!
    class EfficiencyReport
    {
        friend class MinerStatus;

    public:
        uint64_t masked_time_intervals_covered = 0;
        uint64_t masked_time_intervals_elapsed = 0;
        double actual_cumulative_weight = 0.0;
        double ideal_cumulative_weight = 0.0;

        //!
        //! \brief Get a metric that describes the efficiency of the miner
        //! loop.
        //!
        //! \return The percentage of the number of possible kernel search
        //! intervals that the miner loop examined since boot.
        //!
        double StakingLoopEfficiency() const;

        //!
        //! \brief Get a metric that describes the overall efficiency of the
        //! weight considered by the miner.
        //!
        //! \return The percentage of the ideal stake weight considered over
        //! the kernel search intervals executed since boot.
        //!
        double StakingEfficiency() const;

    private:
        //!
        //! \brief Apply information from the last kernel search for efficiency
        //! calculations.
        //!
        void UpdateMetrics(uint64_t weight_sum, int64_t balance_weight);
    }; // EfficiencyReport

    //!
    //! \brief Initialize an empty miner status object (all fields zeroed).
    //!
    MinerStatus();

    //!
    //! \brief Get a string representation of a set of miner status errors.
    //!
    //! \param error_flags The errors to produce a string for.
    //!
    //! \return A semicolon-delimited string of error descriptions, or an empty
    //! string if the provided flags represent no errors.
    //!
    static std::string FormatErrors(ErrorFlags error_flags);

    //!
    //! \brief Determine whether the miner can attempt to stake a block.
    //!
    //! \return \c true if no problems prevent staking.
    //!
    bool StakingEnabled() const;

    //!
    //! \brief Determine whether the miner actively attempts to stake a block.
    //!
    //! \return \c true if the miner thread recently searched for a kernel.
    //!
    bool StakingActive() const;

    //!
    //! \brief Get a copy of the last kernel search information stored in the
    //! miner status.
    //!
    SearchReport GetSearchReport() const;

    //!
    //! \brief Get a copy of the miner efficiency metrics stored in the miner
    //! status.
    //!
    EfficiencyReport GetEfficiencyReport() const;

    //!
    //! \brief Get a string representation of a set of miner status errors.
    //!
    //! \return A semicolon-delimited string of error descriptions, or an empty
    //! string if the miner status does not contain errors.
    //!
    std::string FormatErrors() const;

    //!
    //! \brief Append a staking problem to the status.
    //!
    //! \param error_flags Represents one or more problems to include.
    //!
    void AddError(ErrorFlags error_flag);

    //!
    //! \brief Set the current staking problems and broadcast a miner status
    //! update for the GUI.
    //!
    //! \param error_flags Represents one or more problems to include.
    //!
    void UpdateCurrentErrors(ErrorFlags error_flags);

    //!
    //! \brief Increase the "blocks created" metric by one. Called by the miner
    //! in an intermediate step.
    //!
    void IncrementBlocksCreated();

    //!
    //! \brief Save the information from the miner's last kernel search cycle
    //! and broadcast a miner status update for the GUI.
    //!
    //! \param kernel_found     \c true if the miner solved a kernel proof.
    //! \param search_timestamp Timestamp of the last kernel search in seconds.
    //! \param block_version    The current miner block format.
    //! \param weight_sum       Total kernel weight of staked UTXOs.
    //! \param value_sum        Total amount of staked UTXOs in GRC.
    //! \param weight_min       Smallest weight of a UTXO searched.
    //! \param weight_max       Greatest weight of a UTXO searched.
    //! \param balance_weight   Total weight of balance for efficiency metrics.
    //!
    void UpdateLastSearch(
        bool kernel_found,
        int64_t search_timestamp,
        int block_version,
        uint64_t weight_sum,
        double value_sum,
        uint64_t weight_min,
        uint64_t weight_max,
        int64_t balance_weight);

    //!
    //! \brief Remember the last successful coinstake transaction and increment
    //! the accepted blocks counter.
    //!
    void UpdateLastStake(const uint256& coinstake_hash);

    //!
    //! \brief Zero-out the metrics of the last staking search cycle and
    //! broadcast a miner status update for the GUI.
    //!
    void ClearLastSearch();

    //!
    //! \brief Invalidate the cached coinstake transaction hash of the node's
    //! most recent staked block.
    //!
    void ClearLastStake();

    //!
    //! \brief Remove any tracked staking problem statuses.
    //!
    void ClearErrors();

private:
    //!
    //! \brief Guards most operations except for the atomic error flags.
    //!
    mutable CCriticalSection m_cs;

    //!
    //! \brief A set of bits that represent the current staking problems.
    //!
    //! The error flags are mostly accessed independently of the other data, so
    //! we use an atomic object for thread safety instead of the mutex.
    //!
    std::atomic<ErrorFlags> m_error_flags;

    //!
    //! \brief The hash of the last coinstake transaction for a block mined
    //! successfully by this node.
    //!
    uint256 m_last_pos_tx_hash;

    //!
    //! \brief Contains the miner status information related to the last kernel
    //! search cycle performed by the miner thread.
    //!
    SearchReport m_search;

    //!
    //! \brief Contains the miner status information needed to calculate the
    //! efficiency of the miner thread.
    //!
    EfficiencyReport m_efficiency;
}; // MinerStatus
} // namespace GRC

extern GRC::MinerStatus g_miner_status;
