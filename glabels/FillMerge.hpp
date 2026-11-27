//  FillMerge.hpp
//
//  Copyright (C) 2014-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
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

#ifndef FillMerge_hpp
#define FillMerge_hpp


#include "merge/MemoryMerge.hpp"


namespace glabels
{

        ///
        /// In-memory merge backend used by the Fill page.
        ///
        /// Holds a list of records (built from the fill-jobs table) and serves them to
        /// Merge::setSource()'s read loop.  The record handling lives in merge::MemoryMerge,
        /// which the glabelsneo command line tool shares; this subclass only gives the Fill
        /// page's merge its own identity so FillView can recognise it.  Not registered in
        /// the merge::Factory -- instantiated directly by FillView.
        ///
        class FillMerge : public merge::MemoryMerge
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                FillMerge();
                FillMerge( const FillMerge* other );
                ~FillMerge() override = default;


                /////////////////////////////////
                // Object duplication
                /////////////////////////////////
        public:
                FillMerge* clone() const override;
        };

}


#endif // FillMerge_hpp
