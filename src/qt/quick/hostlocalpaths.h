// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_HOSTLOCALPATHS_H
#define BITCOIN_QT_QUICK_HOSTLOCALPATHS_H

#include <QString>

#include <functional>

//! Paths on the machine the GUI runs on that the module cannot compute itself
//! without core headers. The composition root hands in a probe for each; the
//! probe runs when a path is asked for, so it sees the arguments as they are
//! then. Owned by QmlProvider.
class HostLocalPaths
{
public:
    //! The configuration file's path; an empty function clears the probe.
    void setConfigFileProbe(std::function<QString()> probe);
    //! Whether a configuration file probe is set.
    bool available() const;
    //! The probe's answer, or an empty string when no probe is set.
    QString configFile() const;

private:
    std::function<QString()> m_config_file;
};

#endif // BITCOIN_QT_QUICK_HOSTLOCALPATHS_H
