//  InstallLayout.hpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef update_InstallLayout_hpp
#define update_InstallLayout_hpp


#include <QList>
#include <QString>


namespace glabels::update
{

        enum class Os { Windows, Linux, MacOs, Other };

        /// The OS this binary was built for.
        Os currentOs();

        /// Key of this platform's package in the update manifest ("windows-x86_64", "linux-x86_64",
        /// "macos-arm64"), or an empty string if no package exists for this platform.
        QString platformKey();


        ///
        /// Where the running program is installed and what an update has to replace.
        ///
        /// A release package is a relocatable directory (Windows zip, Linux tarball) or one or two
        /// application bundles (macOS).  An update is staged next to the install, in a hidden sibling
        /// ".<name>.update", so that going live is a same-volume directory rename.
        ///
        struct InstallLayout
        {
                /// One directory (or .app bundle) that is replaced as a whole.
                struct Unit
                {
                        QString installed;   ///< e.g. C:/Tools/glabels-neo
                        QString staged;      ///< e.g. C:/Tools/.glabels-neo.update
                        QString backup;      ///< e.g. C:/Tools/.glabels-neo.old   (only exists during the swap)
                };

                bool          supported{false};
                QString       reason;           ///< why not, when !supported (shown to the user)
                QList<Unit>   units;
                QString       helperPath;       ///< glabels-updater inside the installed copy
                QString       relaunchPath;     ///< what to start to run the program again
                QString       mainRel;          ///< main program, relative to units[0] (must exist in a staged copy)
                QString       helperRel;        ///< updater helper, relative to units[0]
                QString       smokeRel;         ///< program for the smoke test ("--Version"), relative to units[0]
                Os            os{Os::Other};

                /// Work out the layout from the path of the running executable.  `probeWritable` makes
                /// it actually test that the install location can be replaced (create/rename/remove a
                /// scratch directory beside it).
                static InstallLayout detect( const QString& executablePath, Os os, bool probeWritable = true );

                /// Check that the staged copies are complete and self-contained.  Returns a description
                /// of the first problem, or an empty string if they are fine.
                QString validateStaged() const;
        };

}


#endif // update_InstallLayout_hpp
