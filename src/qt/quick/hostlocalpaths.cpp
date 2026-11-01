// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/hostlocalpaths.h>

#include <utility>

void HostLocalPaths::setConfigFileProbe(std::function<QString()> probe)
{
    m_config_file = std::move(probe);
}

bool HostLocalPaths::available() const
{
    return static_cast<bool>(m_config_file);
}

QString HostLocalPaths::configFile() const
{
    return m_config_file ? m_config_file() : QString();
}
