//  AboutDialog.cpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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


#include "AboutDialog.hpp"

#include "AppearanceModel.hpp"

#include "model/Branding.hpp"
#include "model/Version.hpp"

#include <QDebug>
#include <QDesktopServices>
#include <QUrl>


namespace glabels
{

        ///
        /// Constructor
        ///
        AboutDialog::AboutDialog( QWidget *parent )
                : QDialog(parent)
        {
                setupUi( this );

                if ( AppearanceModel::style() != AppearanceModel::STYLE_DARK )
                {
                        titleLabel->setPixmap( QPixmap( ":images/glabels-label-designer.png" ) );
                }
                else
                {
                        titleLabel->setPixmap( QPixmap( ":images/glabels-label-designer-dark.png" ) );
                }

                QString version = tr("Version") + " " + model::Version::LONG_STRING;

                QString description = tr("A program to create labels and business cards.");

                QString copyright = "Copyright &copy; 2016-2026 Jaye Evins <evins@snaught.com>";

                QString licenseParagraph1 =
                        tr( "gLabels is free software: you can redistribute it and/or modify "
                            "it under the terms of the GNU General Public License as published by "
                            "the Free Software Foundation, either version 3 of the License, or "
                            "(at your option) any later version." );

                QString licenseParagraph2 =
                        tr( "gLabels is distributed in the hope that it will be useful, "
                            "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
                            "GNU General Public License for more details." );

                QString productName = "<b>" + model::Branding::DISPLAY_NAME.toHtmlEscaped() + "</b>";

                // Attribution is shown wherever the product name is shown.  It is deliberately NOT
                // passed through tr(): it names the upstream project and must not be rewritten.
                QString attribution =
                        model::Branding::ATTRIBUTION.toHtmlEscaped() +
                        " &mdash; <a href='" + model::Branding::UPSTREAM_URL + "'>" +
                        model::Branding::UPSTREAM_URL.toHtmlEscaped() + "</a>";

                QString forkCopyright = "Copyright &copy; 2026 gLabels Neo contributors";

                QString markup =
                        "<p align='center'>" + productName + "</p>" +
                        "<p align='center'>" + version + "</p>" +
                        "<p align='center'>" + description + "</p>" +
                        "<p align='center'>" + copyright + "<br/>" + forkCopyright + "</p>" +
                        "<p align='center'>" + attribution + "</p>" +
                        "<p align='left'>"   + licenseParagraph1 + "</p>" +
                        "<p align='left'>"   + licenseParagraph2 + "</p>";

                aboutLabel->setText( markup );
        }


        ///
        /// "License" Button Clicked Slot
        ///
        void AboutDialog::onLicenseButtonClicked()
        {
                QDesktopServices::openUrl( QUrl("http://www.gnu.org/licenses/gpl-3.0.txt") );
        }


        ///
        /// "Website" Button Clicked Slot
        ///
        void AboutDialog::onWebsiteButtonClicked()
        {
                QDesktopServices::openUrl( QUrl(model::Version::WEBSITE) );
        }

} // namespace glabels
