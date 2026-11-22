//  BrandTranslator.hpp
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

#ifndef BrandTranslator_hpp
#define BrandTranslator_hpp


#include "model/Branding.hpp"

#include <QString>
#include <QTranslator>
#include <vector>


namespace glabels
{

        ///
        /// Translator that sits in front of the real translation catalogs and rewrites the product
        /// token "gLabels" to the display name "gLabels Neo" in every user-visible string.
        ///
        /// This lets us rebrand all UI text, in every language, without touching any .ts/.ui source
        /// string (which would mark ~1,100 existing translations as unfinished).
        ///
        /// The wrapped translators are loaded by the caller but must NOT be installed themselves;
        /// only this wrapper is installed.  Earlier entries take precedence.
        ///
        class BrandTranslator : public QTranslator
        {
        public:
                explicit BrandTranslator( std::vector<QTranslator*> sources )
                        : mSources( std::move(sources) )
                {
                }

                QString translate( const char* context,
                                   const char* sourceText,
                                   const char* disambiguation = nullptr,
                                   int         n = -1 ) const override
                {
                        for ( const QTranslator* source : mSources )
                        {
                                QString result = source->translate( context, sourceText, disambiguation, n );
                                if ( !result.isNull() )
                                {
                                        return model::Branding::rebrand( result );
                                }
                        }

                        // Not translated (e.g. C locale): still rebrand the source text.
                        return model::Branding::rebrand( QString::fromUtf8( sourceText ) );
                }

                /// QCoreApplication::installTranslator() refuses empty translators.
                bool isEmpty() const override
                {
                        return false;
                }

        private:
                std::vector<QTranslator*> mSources;
        };

}


#endif // BrandTranslator_hpp
