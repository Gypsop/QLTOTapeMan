/*
 * QLTOTapeMan - Qt-based LTO Tape Manager
 * libqltfs - LTFS Core Library
 *
 * Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
 * https://github.com/Gypsop/QLTOTapeMan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QtCore/qglobal.h>

/**
 * @brief Library export/import macro for libqltfs
 *
 * Currently the library is built as static, but this macro is prepared
 * for potential future shared library builds.
 */
#if defined(LIBQLTFS_LIBRARY)
#  define LIBQLTFS_EXPORT Q_DECL_EXPORT
#else
#  define LIBQLTFS_EXPORT Q_DECL_IMPORT
#endif

/**
 * @brief libqltfs namespace
 *
 * All classes and functions in the libqltfs library are contained
 * within the qltfs namespace to avoid naming conflicts.
 */
namespace qltfs {

/**
 * @brief Library version information
 */
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

/**
 * @brief Get library version string
 * @return Version string in format "major.minor.patch"
 */
inline QString versionString()
{
    return QStringLiteral("%1.%2.%3")
        .arg(VERSION_MAJOR)
        .arg(VERSION_MINOR)
        .arg(VERSION_PATCH);
}

} // namespace qltfs
