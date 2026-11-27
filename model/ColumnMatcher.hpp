//  ColumnMatcher.hpp
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

#ifndef model_ColumnMatcher_hpp
#define model_ColumnMatcher_hpp


#include <QMap>
#include <QString>
#include <QStringList>


namespace glabels::model
{

        ///
        /// Which source column fills a label field, and how that was decided.
        ///
        struct ColumnMatch
        {
                enum class Kind
                {
                        None,             ///< no column fills the field
                        Blank,            ///< the person chose "leave blank"
                        Mapped,           ///< the person chose the column (ExternalDataSource::columnMap)
                        Exact,            ///< same name
                        CaseInsensitive,  ///< same name, ignoring case
                        Normalized,       ///< same name ignoring case, spaces, "_", "-" and punctuation
                        Suggested         ///< a likely column; NOT applied until the person accepts it
                };

                Kind    kind{ Kind::None };
                QString column;           ///< the column (Suggested: the one we would suggest)

                /// True when the column actually fills the field.
                bool isApplied() const
                {
                        return kind == Kind::Mapped          ||
                               kind == Kind::Exact           ||
                               kind == Kind::CaseInsensitive ||
                               kind == Kind::Normalized;
                }
        };


        ///
        /// Decides which column of a data source fills each field of a label.
        ///
        /// Order of preference: an explicit choice, the same name, the same name ignoring case,
        /// the same name ignoring spaces and punctuation.  Anything looser is only ever a
        /// suggestion, so a messy header row can never silently feed the wrong data.
        ///
        class ColumnMatcher
        {
        public:
                /// Lower case, letters and digits only ("Unit price", "unit_price" and "UnitPrice" agree).
                static QString normalize( const QString& name );

                /// Best match for one field.
                static ColumnMatch match( const QString&                field,
                                          const QStringList&            columns,
                                          const QMap<QString,QString>&  columnMap );

                /// Matches for several fields.
                static QMap<QString,ColumnMatch> matchAll( const QStringList&            fields,
                                                           const QStringList&            columns,
                                                           const QMap<QString,QString>&  columnMap );

                /// Short human wording for a kind, e.g. "Matched by name".
                static QString describe( ColumnMatch::Kind kind );

                /// Columns that fill none of the given fields (Qty and Print columns excluded).
                static QStringList unusedColumns( const QStringList&                  columns,
                                                  const QMap<QString,ColumnMatch>&    matches );

                /// True for the columns that carry the quantity or the print flag, not a label field.
                static bool isControlColumn( const QString& column );
        };

}


#endif // model_ColumnMatcher_hpp
