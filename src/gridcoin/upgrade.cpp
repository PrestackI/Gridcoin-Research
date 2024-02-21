// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/upgrade.h"
#include <crypto/sha256.h>
#include "util.h"
#include "init.h"

#include <algorithm>
#include <stdexcept>
#include <univalue.h>
#include <vector>
#include <boost/thread.hpp>
#include <boost/exception/exception.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <iostream>

#include <zip.h>

using namespace GRC;

SnapshotExtractStatus GRC::ExtractStatus;

bool GRC::fCancelOperation = false;

Upgrade::Upgrade()
{
    DownloadStatus.Reset();
    ExtractStatus.Reset();
}

void Upgrade::ScheduledUpdateCheck()
{
    std::string VersionResponse;
    std::string change_log;

    Upgrade::UpgradeType upgrade_type {Upgrade::UpgradeType::Unknown};

    CheckForLatestUpdate(VersionResponse, change_log, upgrade_type);
}

bool Upgrade::CheckForLatestUpdate(std::string& client_message_out, std::string& change_log, Upgrade::UpgradeType& upgrade_type,
                                   bool ui_dialog, bool snapshotrequest)
{
    // If testnet skip this || If the user changes this to disable while wallet running just drop out of here now.
    // (Need a way to remove items from scheduler.)
    if (fTestNet || (gArgs.GetBoolArg("-disableupdatecheck", false) && !snapshotrequest))
        return false;

    Http VersionPull;

    std::string GithubResponse;
    std::string VersionResponse;

    // We receive the response and it's in a json reply
    UniValue Response(UniValue::VOBJ);

    try
    {
        VersionResponse = VersionPull.GetLatestVersionResponse();
    }

    catch (const std::runtime_error& e)
    {
        return error("%s: Exception occurred while checking for latest update. (%s)", __func__, e.what());
    }

    if (VersionResponse.empty())
    {
        LogPrintf("WARNING: %s: No Response from GitHub", __func__);

        return false;
    }

    std::string GithubReleaseData;
    std::string GithubReleaseTypeData;
    std::string GithubReleaseBody;
    std::string GithubReleaseType;

    try
    {
        Response.read(VersionResponse);

        // Get the information we need:
        // 'body' for information about changes
        // 'tag_name' for version
        // 'name' for checking if it is a mandatory or leisure
        GithubReleaseData = find_value(Response, "tag_name").get_str();
        GithubReleaseTypeData = find_value(Response, "name").get_str();
        GithubReleaseBody = find_value(Response, "body").get_str();
    }

    catch (std::exception& ex)
    {
        error("%s: Exception occurred while parsing json response (%s)", __func__, ex.what());

        return false;
    }

    GithubReleaseTypeData = ToLower(GithubReleaseTypeData);

    if (GithubReleaseTypeData.find("leisure") != std::string::npos) {
        GithubReleaseType = _("leisure");
        upgrade_type = Upgrade::UpgradeType::Leisure;
    } else if (GithubReleaseTypeData.find("mandatory") != std::string::npos) {
        GithubReleaseType = _("mandatory");
        // This will be confirmed below by also checking the second position version. If not incremented, then it will
        // be set to unknown.
        upgrade_type = Upgrade::UpgradeType::Mandatory;
    } else {
        GithubReleaseType = _("unknown");
        upgrade_type = Upgrade::UpgradeType::Unknown;
    }

    // Parse version data
    std::vector<std::string> GithubVersion;
    std::vector<int> LocalVersion;

    ParseString(GithubReleaseData, '.', GithubVersion);

    LocalVersion.push_back(CLIENT_VERSION_MAJOR);
    LocalVersion.push_back(CLIENT_VERSION_MINOR);
    LocalVersion.push_back(CLIENT_VERSION_REVISION);
    LocalVersion.push_back(CLIENT_VERSION_BUILD);

    if (GithubVersion.size() != 4)
    {
        error("%s: Got malformed version (%s)", __func__, GithubReleaseData);

        return false;
    }

    bool NewVersion = false;
    bool NewMandatory = false;
    bool same_version = true;

    try {
        // Left to right version numbers.
        // 4 numbers to check.
        for (unsigned int x = 0; x <= 3; x++) {
            int github_version = 0;

            if (!ParseInt32(GithubVersion[x], &github_version)) {
                throw std::invalid_argument("Failed to parse GitHub version from official GitHub project repo.");
            }

            if (github_version > LocalVersion[x]) {
                NewVersion = true;
                same_version = false;

                if (x < 2 && upgrade_type == Upgrade::UpgradeType::Mandatory) {
                    NewMandatory = true;
                } else {
                    upgrade_type = Upgrade::UpgradeType::Unknown;
                }
            } else {
                same_version &= (github_version == LocalVersion[x]);
            }
        }
    } catch (std::exception& ex) {
        error("%s: Exception occurred checking client version against GitHub version (%s)",
                  __func__, ToString(ex.what()));

        upgrade_type = Upgrade::UpgradeType::Unknown;
        return false;
    }

    // Populate client_message_out regardless of whether new version is found, because we are using this method for
    // the version information button in the "About Gridcoin" dialog.
    client_message_out = _("Local version: ") + strprintf("%d.%d.%d.%d", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR,
                                                          CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD) + "\r\n";
    client_message_out.append(_("GitHub version: ") + GithubReleaseData + "\r\n");

    if (NewVersion) {
        client_message_out.append(_("This update is ") + GithubReleaseType + "\r\n\r\n");
    } else if (same_version) {
        client_message_out.append(_("The latest release is ") + GithubReleaseType + "\r\n\r\n");
        client_message_out.append(_("You are running the latest release.") + "\n");
    } else {
        client_message_out.append(_("The latest release is ") + GithubReleaseType + "\r\n\r\n");

        // If not a new version available and the version is not the same, the only thing left is that we are running
        // a version greater than the latest release version, so set the upgrade_type to Unsupported, which is used for a
        // warning.
        upgrade_type = Upgrade::UpgradeType::Unsupported;
        client_message_out.append(_("WARNING: You are running a version that is higher than the latest release.") + "\n");
    }

    change_log = GithubReleaseBody;

    if (!NewVersion) return false;

    // For snapshot requests we will only return true if there is a new mandatory version AND the snapshotrequest boolean
    // is set true. This is because the snapshot request context is looking for the presence of a new mandatory to block
    // the snapshot download before upgrading to the new mandatory if there is one.
    if (snapshotrequest && NewMandatory) return true;

    if (NewMandatory) {
        client_message_out.append(_("WARNING: A mandatory release is available. Please upgrade as soon as possible.")
                                  + "\n");
    }

    if (ui_dialog) {
        uiInterface.UpdateMessageBox(client_message_out, static_cast<int>(upgrade_type), change_log);
    }

    return true;
}

void Upgrade::SnapshotMain()
{
    std::cout << std::endl;
    std::cout << _("Snapshot Process Has Begun.") << std::endl;
    std::cout << _("Warning: Ending this process after Stage 2 will result in syncing from 0 or an "
                   "incomplete/corrupted blockchain.") << std::endl << std::endl;

    // Verify a mandatory release is not available before we continue to snapshot download.
    std::string VersionResponse = "";
    std::string change_log;
    Upgrade::UpgradeType upgrade_type {Upgrade::UpgradeType::Unknown};

    if (CheckForLatestUpdate(VersionResponse, change_log, upgrade_type, false, true))
    {
        std::cout << this->ResetBlockchainMessages(UpdateAvailable) << std::endl;
        std::cout << this->ResetBlockchainMessages(GithubResponse) << std::endl;
        std::cout << VersionResponse << std::endl;

        throw std::runtime_error(_("Failed to download snapshot as mandatory client is available for download."));
    }

    Progress progress;

    progress.SetType(Progress::Type::SnapshotDownload);

    // Create a worker thread to do all of the heavy lifting. We are going to use a ping-pong workflow state here,
    // with progress Type as the trigger.
    boost::thread WorkerMainThread(std::bind(&Upgrade::WorkerMain, boost::ref(progress)));

    while (!DownloadStatus.GetSnapshotDownloadComplete())
    {
        if (DownloadStatus.GetSnapshotDownloadFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            throw std::runtime_error("Failed to download snapshot.zip; See debug.log");
        }

        if (progress.Update(DownloadStatus.GetSnapshotDownloadProgress(), DownloadStatus.GetSnapshotDownloadSpeed(),
                        DownloadStatus.GetSnapshotDownloadAmount(), DownloadStatus.GetSnapshotDownloadSize()))
        {
            std::cout << progress.Status() << std::flush;
        }

        UninterruptibleSleep(std::chrono::milliseconds{1000});
    }

    // This is needed in some spots as the download can complete before the next progress update occurs so just 100% here
    // as it was successful
    if (progress.Update(100, -1, DownloadStatus.GetSnapshotDownloadSize(), DownloadStatus.GetSnapshotDownloadSize()))
    {
        std::cout << progress.Status() << std::flush;
    }

    std::cout << std::endl;

    progress.SetType(Progress::Type::SHA256SumVerification);

    while (!DownloadStatus.GetSHA256SUMComplete())
    {
        if (DownloadStatus.GetSHA256SUMFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            throw std::runtime_error("Failed to verify SHA256SUM of snapshot.zip; See debug.log");
        }

        if (progress.Update(DownloadStatus.GetSHA256SUMProgress()))
        {
            std::cout << progress.Status() << std::flush;
        }

        UninterruptibleSleep(std::chrono::milliseconds{1000});
    }

    if (progress.Update(100)) std::cout << progress.Status() << std::flush;

    std::cout << std::endl;

    progress.SetType(Progress::Type::CleanupBlockchainData);

    while (!DownloadStatus.GetCleanupBlockchainDataComplete())
    {
        if (DownloadStatus.GetCleanupBlockchainDataFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            throw std::runtime_error("Failed to cleanup previous blockchain data prior to extraction of snapshot.zip; "
                                     "See debug.log");
        }

        if (progress.Update(DownloadStatus.GetCleanupBlockchainDataProgress()))
        {
            std::cout << progress.Status() << std::flush;
        }

        UninterruptibleSleep(std::chrono::milliseconds{1000});
    }

    if (progress.Update(100)) std::cout << progress.Status() << std::flush;

    std::cout << std::endl;

    progress.SetType(Progress::Type::SnapshotExtraction);

    while (!ExtractStatus.GetSnapshotExtractComplete())
    {
        if (ExtractStatus.GetSnapshotExtractFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            // Do this without checking on success, If it passed in stage 3 it will pass here.
            CleanupBlockchainData();

            throw std::runtime_error("Failed to extract snapshot.zip; See debug.log");
        }

        if (progress.Update(ExtractStatus.GetSnapshotExtractProgress()))
            std::cout << progress.Status() << std::flush;

        UninterruptibleSleep(std::chrono::milliseconds{1000});
    }

    if (progress.Update(100)) std::cout << progress.Status() << std::flush;

    std::cout << std::endl;

    // This interrupt-join needs to be here to ensure the WorkerMain interrupts the while loop and collapses before the
    // Progress object that was passed to it is destroyed.
    WorkerMainThread.interrupt();
    WorkerMainThread.join();

    std::cout << _("Snapshot Process Complete!") << std::endl;

    return;
}

void Upgrade::WorkerMain(Progress& progress)
{
    // The "steps" are triggered in SnapshotMain but processed here in this switch statement.
    bool finished = false;

    while (!finished && !fCancelOperation)
    {
        boost::this_thread::interruption_point();

        switch (progress.GetType())
        {
        case Progress::Type::SnapshotDownload:
            if (DownloadStatus.GetSnapshotDownloadFailed())
            {
                finished = true;
                return;
            }
            else if (!DownloadStatus.GetSnapshotDownloadComplete())
            {
                DownloadSnapshot();
            }
            break;
        case Progress::Type::SHA256SumVerification:
            if (DownloadStatus.GetSHA256SUMFailed())
            {
                finished = true;
                return;
            }
            else if (!DownloadStatus.GetSHA256SUMComplete())
            {
                 VerifySHA256SUM();
            }
            break;
        case Progress::Type::CleanupBlockchainData:
            if (DownloadStatus.GetCleanupBlockchainDataFailed())
            {
                finished = true;
                return;
            }
            else if (!DownloadStatus.GetCleanupBlockchainDataComplete())
            {
                CleanupBlockchainData();
            }
            break;
        case Progress::Type::SnapshotExtraction:
            if (ExtractStatus.GetSnapshotExtractFailed())
            {
                finished = true;
                return;
            }
            else if (!ExtractStatus.GetSnapshotExtractComplete())
            {
                ExtractSnapshot();
            }
        }

        UninterruptibleSleep(std::chrono::milliseconds{1000});
    }
}

void Upgrade::DownloadSnapshot()
{
     // Download the snapshot.zip
    Http HTTPHandler;

    try
    {
        HTTPHandler.DownloadSnapshot();
    }
    catch(std::runtime_error& e)
    {
        error("%s: Exception occurred while attempting to download snapshot (%s)", __func__, e.what());

        DownloadStatus.SetSnapshotDownloadFailed(true);

        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Snapshot download complete.", __func__);

    DownloadStatus.SetSnapshotDownloadComplete(true);

    return;
}

void Upgrade::VerifySHA256SUM()
{
    Http HTTPHandler;

    std::string ServerSHA256SUM = "";

    try
    {
        ServerSHA256SUM = HTTPHandler.GetSnapshotSHA256();
    }
    catch (std::runtime_error& e)
    {
        error("%s: Exception occurred while attempting to retrieve snapshot SHA256SUM (%s)",
              __func__, e.what());

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }

    if (ServerSHA256SUM.empty())
    {
        error("%s: Empty SHA256SUM returned from server.", __func__);

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }

    CSHA256 hasher;

    fs::path fileloc = GetDataDir() / "snapshot.zip";
    uint8_t buffer[32768];
    int bytesread = 0;

    CAutoFile file(fsbridge::fopen(fileloc, "rb"), SER_DISK, CLIENT_VERSION);

    if (file.IsNull())
    {
        error("%s: Failed to open snapshot.zip.", __func__);

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }

    unsigned int total_reads = fs::file_size(fileloc) / sizeof(buffer) + 1;

    unsigned int read_count = 0;
    while ((bytesread = fread(buffer, 1, sizeof(buffer), file.Get())))
    {
        hasher.Write(buffer, bytesread);
        ++read_count;

        DownloadStatus.SetSHA256SUMProgress(read_count * 100 / total_reads);
    }

    uint8_t digest[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(digest);

    const std::vector<unsigned char> digest_vector(digest, digest + CSHA256::OUTPUT_SIZE);

    std::string FileSHA256SUM = HexStr(digest_vector);

    if (ServerSHA256SUM == FileSHA256SUM)
    {
        LogPrint(BCLog::LogFlags::VERBOSE, "INFO %s: SHA256SUM verification successful (Server = %s, File = %s).",
                 __func__, ServerSHA256SUM, FileSHA256SUM);

        DownloadStatus.SetSHA256SUMProgress(100);
        DownloadStatus.SetSHA256SUMComplete(true);

        return;
    }
    else
    {
        error("%s: Mismatch of SHA256SUM of snapshot.zip (Server = %s / File = %s)",
              __func__, ServerSHA256SUM, FileSHA256SUM);

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }
}

bool Upgrade::GetActualCleanupPath(fs::path& actual_cleanup_path)
{
    actual_cleanup_path = GetDataDir();

    // This is required because of problems with junction point handling in the boost filesystem library. Please see
    // https://github.com/boostorg/filesystem/issues/125. We are not quite ready to switch over to std::filesystem yet.
    // 1. I don't know whether the issue is fixed there, and
    // 2. Not all C++17 compilers have the filesystem headers, since this was merged from boost in 2017.
    //
    // I don't believe it is very common for Windows users to redirect the Gridcoin data directory with a junction point,
    // but it is certainly possible. We should handle it as gracefully as possible.
    if (fs::is_symlink(actual_cleanup_path))
    {
        LogPrintf("INFO: %s: Data directory is a symlink.",
                  __func__);

        try
        {
            LogPrintf("INFO: %s: True path for the symlink is %s.", __func__, fs::read_symlink(actual_cleanup_path).string());

            actual_cleanup_path = fs::read_symlink(actual_cleanup_path);
        }
        catch (fs::filesystem_error &ex)
        {
            error("%s: The data directory symlink or junction point cannot be resolved to the true canonical path. "
                  "This can happen on Windows. Please change the data directory specified to the actual true path "
                  "using the  -datadir=<path> option and try again.", __func__);

            DownloadStatus.SetCleanupBlockchainDataFailed(true);

            return false;
        }
    }

    return true;
}

void Upgrade::CleanupBlockchainData(bool include_blockchain_data_files)
{
    fs::path CleanupPath;

    if (!GetActualCleanupPath(CleanupPath)) return;

    unsigned int total_items = 0;
    unsigned int items = 0;

    // We must delete previous blockchain data
    // txleveldb
    // accrual
    // blk*.dat
    fs::directory_iterator IterEnd;

    // Count for progress bar first
    try
    {
        for (fs::directory_iterator Iter(CleanupPath); Iter != IterEnd; ++Iter)
        {
            if (fs::is_directory(Iter->path()))
            {
                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "txleveldb")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();
                         ++it)
                    {
                        ++total_items;
                    }
                }

                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "accrual")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();
                         ++it)
                    {
                        ++total_items;
                    }
                }

                // If it was a directory no need to check if a regular file below.
                continue;
            }

            else if (fs::is_regular_file(*Iter) && include_blockchain_data_files)
            {
                size_t FileLoc = Iter->path().filename().string().find("blk");

                if (FileLoc != std::string::npos)
                {
                    std::string filetocheck = Iter->path().filename().string();

                    // Check it ends with .dat and starts with blk
                    if (filetocheck.substr(0, 3) == "blk" && filetocheck.substr(filetocheck.length() - 4, 4) == ".dat")
                    {
                        ++total_items;
                    }
                }
            }
        }
    }
    catch (fs::filesystem_error &ex)
    {
        error("%s: Exception occurred: %s", __func__, ex.what());

        DownloadStatus.SetCleanupBlockchainDataFailed(true);

        return;
    }

    if (!total_items)
    {
        // Nothing to clean up!

        DownloadStatus.SetCleanupBlockchainDataComplete(true);

        return;
    }

    // Now try the cleanup.
    try
    {
        // Remove the files. We iterate as we know blk* will exist more and more in future as well
        for (fs::directory_iterator Iter(CleanupPath); Iter != IterEnd; ++Iter)
        {
            if (fs::is_directory(Iter->path()))
            {
                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "txleveldb")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();)
                    {
                        fs::path filepath = *it++;

                        if (fs::remove(filepath))
                        {
                            ++items;
                            DownloadStatus.SetCleanupBlockchainDataProgress(items * 100 / total_items);
                        }
                        else
                        {
                            DownloadStatus.SetCleanupBlockchainDataFailed(true);

                            return;
                        }
                    }
                }

                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "accrual")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();)
                    {
                        fs::path filepath = *it++;

                        if (fs::remove(filepath))
                        {
                            ++items;
                            DownloadStatus.SetCleanupBlockchainDataProgress(items * 100 / total_items);
                        }
                        else
                        {
                            DownloadStatus.SetCleanupBlockchainDataFailed(true);

                            return;
                        }
                    }
                }

                // If it was a directory no need to check if a regular file below.
                continue;
            }

            else if (fs::is_regular_file(*Iter) && include_blockchain_data_files)
            {
                size_t FileLoc = Iter->path().filename().string().find("blk");

                if (FileLoc != std::string::npos)
                {
                    std::string filetocheck = Iter->path().filename().string();

                    // Check it ends with .dat and starts with blk
                    if (filetocheck.substr(0, 3) == "blk" && filetocheck.substr(filetocheck.length() - 4, 4) == ".dat")
                    {
                        if (fs::remove(*Iter))
                        {
                            ++items;
                            DownloadStatus.SetCleanupBlockchainDataProgress(items * 100 / total_items);
                        }
                        else
                        {
                            DownloadStatus.SetCleanupBlockchainDataFailed(true);

                            return;
                        }
                    }
                }
            }
        }
    }
    catch (fs::filesystem_error &ex)
    {
        error("%s: Exception occurred: %s", __func__, ex.what());

        DownloadStatus.SetCleanupBlockchainDataFailed(true);

        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Prior blockchain data cleanup successful.", __func__);

    DownloadStatus.SetCleanupBlockchainDataProgress(100);
    DownloadStatus.SetCleanupBlockchainDataComplete(true);

    return;
}

void Upgrade::ExtractSnapshot()
{
    try
    {
        zip_error_t* err = new zip_error_t;
        struct zip* ZipArchive;

        std::string archive_file_string = (GetDataDir() / "snapshot.zip").string();
        const char* archive_file = archive_file_string.c_str();

        int ze;

        ZipArchive = zip_open(archive_file, 0, &ze);
        zip_error_init_with_code(err, ze);

        if (ZipArchive == nullptr)
        {
            ExtractStatus.SetSnapshotExtractFailed(true);

            error("%s: Error opening snapshot.zip as zip archive: %s", __func__, zip_error_strerror(err));

            return;
        }

        fs::path ExtractPath = GetDataDir();
        struct zip_stat ZipStat;
        int64_t lastupdated = GetAdjustedTime();
        long long totaluncompressedsize = 0;
        long long currentuncompressedsize = 0;
        uint64_t entries = (uint64_t) zip_get_num_entries(ZipArchive, 0);

        // Let's scan for total size uncompressed so we can do a detailed progress for the watching user
        for (u_int64_t j = 0; j < entries; ++j)
        {
            if (zip_stat_index(ZipArchive, j, 0, &ZipStat) == 0)
            {
                if (ZipStat.name[strlen(ZipStat.name) - 1] != '/')
                    totaluncompressedsize += ZipStat.size;
            }
        }

        // This protects against a divide by error below and properly returns false
        // if the zip file has no entries.
        if (!totaluncompressedsize)
        {
            ExtractStatus.SetSnapshotZipInvalid(true);

            error("%s: Error - snapshot.zip has no entries", __func__);

            return;
        }

        // Now extract
        for (u_int64_t i = 0; i < entries; ++i)
        {
            if (zip_stat_index(ZipArchive, i, 0, &ZipStat) == 0)
            {
                // Does this require a directory
                if (ZipStat.name[strlen(ZipStat.name) - 1] == '/')
                {
                    fs::create_directory(ExtractPath / ZipStat.name);
                }
                else
                {
                    struct zip_file* ZipFile;

                    ZipFile = zip_fopen_index(ZipArchive, i, 0);

                    if (!ZipFile)
                    {
                        ExtractStatus.SetSnapshotExtractFailed(true);

                        error("%s: Error opening file %s within snapshot.zip", __func__, ZipStat.name);

                        return;
                    }


                    fs::path ExtractFileString = ExtractPath / ZipStat.name;

                    CAutoFile ExtractFile(fsbridge::fopen(ExtractFileString, "wb"), SER_DISK, CLIENT_VERSION);

                    if (ExtractFile.IsNull())
                    {
                        ExtractStatus.SetSnapshotExtractFailed(true);

                        error("%s: Error opening file %s on filesystem", __func__, ZipStat.name);

                        return;
                    }

                    int64_t sum = 0;

                    while ((uint64_t) sum < ZipStat.size)
                    {
                        int64_t len = 0;
                        // Note that using a buffer larger than this risks crashes on macOS.
                        char Buf[256*1024];

                        boost::this_thread::interruption_point();

                        len = zip_fread(ZipFile, &Buf, 256*1024);

                        if (len < 0)
                        {
                            ExtractStatus.SetSnapshotExtractFailed(true);

                            error("%s: Failed to read zip buffer", __func__);

                            return;
                        }

                        fwrite(Buf, 1, (uint64_t) len, ExtractFile.Get());

                        sum += len;
                        currentuncompressedsize += len;

                        // Update Progress every 1 second
                        if (GetAdjustedTime() > lastupdated)
                        {
                            lastupdated = GetAdjustedTime();

                            ExtractStatus.SetSnapshotExtractProgress(currentuncompressedsize * 100
                                                                     / totaluncompressedsize);
                        }
                    }

                    zip_fclose(ZipFile);
                }
            }
        }

        if (zip_close(ZipArchive) == -1)
        {
            ExtractStatus.SetSnapshotExtractFailed(true);

            error("%s: Failed to close snapshot.zip", __func__);

            return;
        }
    }

    catch (boost::thread_interrupted&)
    {
        ExtractStatus.SetSnapshotExtractFailed(true);

        return;
    }

    catch (std::exception& e)
    {
        error("%s: Error occurred during snapshot zip file extraction: %s", __func__, e.what());

        ExtractStatus.SetSnapshotExtractFailed(true);

        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Snapshot.zip extraction successful.", __func__);

    ExtractStatus.SetSnapshotExtractProgress(100);
    ExtractStatus.SetSnapshotExtractComplete(true);
    return;
}

void Upgrade::DeleteSnapshot()
{
    // File is out of scope now check if it exists and if so delete it.
    try
    {
        fs::path snapshotpath = GetDataDir() / "snapshot.zip";

        if (fs::exists(snapshotpath))
            if (fs::is_regular_file(snapshotpath))
                fs::remove(snapshotpath);
    }

    catch (fs::filesystem_error& e)
    {
        LogPrintf("Snapshot Downloader: Exception occurred while attempting to delete snapshot (%s)", e.code().message());
    }
}

bool Upgrade::ResetBlockchainData(bool include_blockchain_data_files)
{
    CleanupBlockchainData(include_blockchain_data_files);

    return (DownloadStatus.GetCleanupBlockchainDataComplete() && !DownloadStatus.GetCleanupBlockchainDataFailed());
}

bool Upgrade::MoveBlockDataFiles(std::vector<std::pair<fs::path, uintmax_t>>& block_data_files)
{
    fs::path cleanup_path;

    if (!GetActualCleanupPath(cleanup_path)) return false;

    fs::directory_iterator IterEnd;

    try {
        for (fs::directory_iterator Iter(cleanup_path); Iter != IterEnd; ++Iter) {
            if (fs::is_regular_file(*Iter)) {
                size_t FileLoc = Iter->path().filename().string().find("blk");

                if (FileLoc != std::string::npos) {
                    std::string filetocheck = Iter->path().filename().string();

                    // Check it ends with .dat and starts with blk
                    if (filetocheck.substr(0, 3) == "blk" && filetocheck.substr(filetocheck.length() - 4, 4) == ".dat") {
                        fs::path new_name = *Iter;
                        new_name.replace_extension(".dat.orig");

                        uintmax_t file_size = fs::file_size(Iter->path());

                        // Rename with orig as the extension, because ProcessBlock will load blocks into a new block data
                        // file.
                        fs::rename(*Iter, new_name);
                        block_data_files.push_back(std::make_pair(new_name, file_size));
                    }
                }
            }
        }
    } catch (fs::filesystem_error &ex) {
        error("%s: Exception occurred: %s. Failed to rename block data files to blk*.dat.orig in preparation for "
              "reindexing.", __func__, ex.what());

        return false;
    }

    return true;
}

bool Upgrade::LoadBlockchainData(std::vector<std::pair<fs::path, uintmax_t>>& block_data_files, bool sort,
                                 bool cleanup_imported_files)
{
    bool successful = true;

    uintmax_t total_size = 0;
    uintmax_t cumulative_size = 0;

    for (const auto& iter : block_data_files) {
        total_size += iter.second;
    }

    if (!total_size) return false;

    // This conditional sort is necessary to allow for two different desired behaviors. -reindex requires the filesystem
    // entries to be sorted, because the order of files in a filesystem iterator in a directory is not guaranteed. On the
    // other hand, for multiple -loadblock arguments, the order of the arguments should be preserved.
    if (sort) std::sort(block_data_files.begin(), block_data_files.end());

    try {
        for (const auto& iter : block_data_files) {

            unsigned int percent_start = cumulative_size * (uintmax_t) 100 / total_size;

            cumulative_size += iter.second;

            unsigned int percent_end = cumulative_size * (uintmax_t) 100 / total_size;

            FILE *block_data_file = fsbridge::fopen(iter.first, "rb");

            LogPrintf("INFO: %s: Loading blocks from %s.", __func__, iter.first.filename().string());

            if (!LoadExternalBlockFile(block_data_file, iter.second, percent_start, percent_end)) {
                successful = false;

                break;
            }
        }
    } catch (fs::filesystem_error &ex) {
        error("%s: Exception occurred: %s. Failure occurred during attempt to load blocks from original "
              "block data file(s).", __func__, ex.what());

        successful = false;
    }

    if (successful) {
        // Only delete the source files that were imported if cleanup_imported_files is set to true
        if (cleanup_imported_files) {
            try {
                for (const auto& iter : block_data_files) {
                    if (!fs::remove(iter.first)) {
                        LogPrintf("WARN: %s: Reindexing of the blockchain was successful; however, one or more of "
                                  "the original block data files (%s) was not able to be deleted. You "
                                  "will have to delete this file manually.", __func__, iter.first.filename().string());
                    }
                }
            }
            catch (fs::filesystem_error &ex) {
                LogPrintf("WARN: %s: Exception occurred: %s. This error occurred while attempting to delete the original "
                          "block data files (blk*.dat.orig). You will have to delete these manually.", __func__, ex.what());
            }
        }
    } else {
        error("%s: A failure occurred during the reindexing of the block data files. The blockchain state is invalid and "
              "you should restart the wallet with the -resetblockchaindata option to clear out the blockchain database "
              "and re-sync the blockchain from the network.", __func__);

        DownloadStatus.SetCleanupBlockchainDataFailed(true);

        return false;
    }

    LogPrintf("INFO: %s: Reindex of the blockchain data was successful.", __func__);

    return true;
}

std::string Upgrade::ResetBlockchainMessages(ResetBlockchainMsg _msg)
{
    std::stringstream stream;

    switch (_msg) {
        case CleanUp:
        {
            stream << _("Datadir: ");
            stream << GetDataDir().string();
            stream << "\r\n\r\n";
            stream << _("Due to the failure to delete the blockchain data you will be required to manually delete the data "
                        "before starting your wallet.");
            stream << "\r\n";
            stream << _("Failure to do so will result in undefined behaviour or failure to start wallet.");
            stream << "\r\n\r\n";
            stream << _("You will need to delete the following.");
            stream << "\r\n\r\n";
            stream << _("Files:");
            stream << "\r\n";
            stream << "blk000*.dat";
            stream << "\r\n\r\n";
            stream << _("Directories:");
            stream << "\r\n";
            stream << "txleveldb";
            stream << "\r\n";
            stream << "accrual";

            break;
        }
        case UpdateAvailable: stream << _("Unable to download a snapshot, as the wallet has detected that a new mandatory "
                                          "version is available for install. The mandatory upgrade must be installed before "
                                          "the snapshot can be downloaded and applied."); break;
        case GithubResponse: stream << _("Latest Version GitHub data response:"); break;
    }

    const std::string& output = stream.str();

    return output;
}
