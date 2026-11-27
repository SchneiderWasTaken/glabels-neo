//  Exit.hpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels Neo, a modified version of gLabels-qt.
//
//  gLabels Neo is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels Neo is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels Neo.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef cli_Exit_hpp
#define cli_Exit_hpp


namespace glabels::cli
{

        ///
        /// Process exit codes.  These are part of the tool's stable machine interface
        /// (see docs/CLI-SPEC.md); do not renumber.
        ///
        enum ExitCode
        {
                ExitOk          = 0,
                ExitInternal    = 1,    ///< unexpected failure
                ExitUsage       = 2,    ///< bad, unknown or conflicting options
                ExitInput       = 3,    ///< label file not found, unreadable or invalid
                ExitData        = 4,    ///< malformed records, over the label cap, or warnings under --strict
                ExitPrinter     = 5,    ///< printer not found, ambiguous, or the job could not be sent
                ExitPartial     = 6,    ///< some jobs printed and some failed (--continue-on-error)
                ExitInterrupted = 130   ///< cancelled at a prompt
        };

}


#endif // cli_Exit_hpp
