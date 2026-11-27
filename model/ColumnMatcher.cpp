//  ColumnMatcher.cpp
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

#include "ColumnMatcher.hpp"

#include <QCoreApplication>
#include <QSet>


namespace glabels::model
{

        QString ColumnMatcher::normalize( const QString& name )
        {
                QString out;
                out.reserve( name.size() );
                for ( const QChar c : name )
                {
                        if ( c.isLetterOrNumber() )
                        {
                                out.append( c.toLower() );
                        }
                }
                return out;
        }


        bool ColumnMatcher::isControlColumn( const QString& column )
        {
                const QString c = column.trimmed();
                return c.compare( QStringLiteral( "Qty" ),   Qt::CaseInsensitive ) == 0 ||
                       c.compare( QStringLiteral( "Print" ), Qt::CaseInsensitive ) == 0;
        }


        ColumnMatch ColumnMatcher::match( const QString&               field,
                                          const QStringList&           columns,
                                          const QMap<QString,QString>& columnMap )
        {
                ColumnMatch m;

                // 1. An explicit choice wins, if the column still exists.
                if ( columnMap.contains( field ) )
                {
                        const QString chosen = columnMap.value( field );
                        if ( chosen.isEmpty() )
                        {
                                m.kind = ColumnMatch::Kind::Blank;
                                return m;
                        }
                        if ( columns.contains( chosen ) )
                        {
                                m.kind   = ColumnMatch::Kind::Mapped;
                                m.column = chosen;
                                return m;
                        }
                        // The chosen column is gone: fall through, so the field can still match by name.
                }

                // 2. Same name.
                if ( columns.contains( field ) )
                {
                        m.kind   = ColumnMatch::Kind::Exact;
                        m.column = field;
                        return m;
                }

                // 3. Same name, ignoring case.
                for ( const QString& c : columns )
                {
                        if ( c.compare( field, Qt::CaseInsensitive ) == 0 )
                        {
                                m.kind   = ColumnMatch::Kind::CaseInsensitive;
                                m.column = c;
                                return m;
                        }
                }

                // 4. Same name, ignoring spaces, "_", "-" and other punctuation.
                const QString nf = normalize( field );
                if ( nf.isEmpty() )
                {
                        return m;
                }
                for ( const QString& c : columns )
                {
                        if ( normalize( c ) == nf )
                        {
                                m.kind   = ColumnMatch::Kind::Normalized;
                                m.column = c;
                                return m;
                        }
                }

                // 5. A likely column, offered but not applied: one name contains the other.
                if ( nf.size() >= 3 )
                {
                        QString best;
                        for ( const QString& c : columns )
                        {
                                const QString nc = normalize( c );
                                if ( nc.size() >= 3 && ( nc.contains( nf ) || nf.contains( nc ) ) )
                                {
                                        if ( best.isEmpty() || c.size() < best.size() )
                                        {
                                                best = c;
                                        }
                                }
                        }
                        if ( !best.isEmpty() )
                        {
                                m.kind   = ColumnMatch::Kind::Suggested;
                                m.column = best;
                        }
                }

                return m;
        }


        QMap<QString,ColumnMatch> ColumnMatcher::matchAll( const QStringList&           fields,
                                                           const QStringList&           columns,
                                                           const QMap<QString,QString>& columnMap )
        {
                QMap<QString,ColumnMatch> out;
                for ( const QString& f : fields )
                {
                        out.insert( f, match( f, columns, columnMap ) );
                }
                return out;
        }


        QString ColumnMatcher::describe( ColumnMatch::Kind kind )
        {
                switch ( kind )
                {
                case ColumnMatch::Kind::Mapped:          return QCoreApplication::translate( "ColumnMatcher", "Chosen" );
                case ColumnMatch::Kind::Exact:           return QCoreApplication::translate( "ColumnMatcher", "Matched by name" );
                case ColumnMatch::Kind::CaseInsensitive: return QCoreApplication::translate( "ColumnMatcher", "Matched by name" );
                case ColumnMatch::Kind::Normalized:      return QCoreApplication::translate( "ColumnMatcher", "Matched by name" );
                case ColumnMatch::Kind::Suggested:       return QCoreApplication::translate( "ColumnMatcher", "Suggested" );
                case ColumnMatch::Kind::Blank:           return QCoreApplication::translate( "ColumnMatcher", "Left blank" );
                case ColumnMatch::Kind::None:
                default:                                 return QCoreApplication::translate( "ColumnMatcher", "Needs a column" );
                }
        }


        QStringList ColumnMatcher::unusedColumns( const QStringList&               columns,
                                                  const QMap<QString,ColumnMatch>& matches )
        {
                QSet<QString> used;
                for ( const ColumnMatch& m : matches )
                {
                        if ( m.isApplied() )
                        {
                                used.insert( m.column );
                        }
                }

                QStringList out;
                for ( const QString& c : columns )
                {
                        if ( !used.contains( c ) && !isControlColumn( c ) )
                        {
                                out.append( c );
                        }
                }
                return out;
        }

}
