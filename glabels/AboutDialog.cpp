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

#include "theme/ThemeIcons.hpp"

#include "model/Branding.hpp"
#include "model/Version.hpp"
#include "update/UpdateConfig.hpp"

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

                // The gLabels Neo mark above the product name (mockup d06); the upstream
                // "gLabels Label Designer" artwork is not shown, the product is gLabels Neo.
                Icon->setPixmap( theme::ThemeIcons::brandMark( 72, devicePixelRatioF() ) );
                titleLabel->hide();

                // The version is the GitHub release this build belongs to, linked to its page:
                //   a release build  -> its tag, e.g. "v1.2.0"
                //   an edge build    -> the rolling "edge" prerelease and the commit it was built from
                //   anything else    -> a local/development build, which is not a GitHub release
                // The build details (snapshot name, commit, date) follow in a second line.
                const QString releasesUrl = "https://github.com/" + update::Config::REPO + "/releases/";
                QString release;
                if ( !update::Config::TAG.isEmpty() )
                {
                        release = "<a href='" + releasesUrl + "tag/" + QString::fromLatin1( QUrl::toPercentEncoding( update::Config::TAG ) ) + "'>" +
                                  update::Config::TAG.toHtmlEscaped() + "</a>";
                }
                else if ( model::Version::IS_DEV_BUILD )
                {
                        // Builds of the dev branch: a rolling prerelease named "dev"
                        release = "<a href='" + releasesUrl + "tag/dev'>" +
                                  tr( "dev" ).toHtmlEscaped() + " " + update::Config::COMMIT.left( 7 ).toHtmlEscaped() + "</a>";
                }
                else if ( update::Config::CHANNEL == "edge" )
                {
                        release = "<a href='" + releasesUrl + "tag/edge'>" +
                                  tr( "edge" ).toHtmlEscaped() + " " + update::Config::COMMIT.left( 7 ).toHtmlEscaped() + "</a>";
                }
                else
                {
                        release = tr( "development build" ).toHtmlEscaped();
                }

                QString version = tr("Version") + " " + release;
                QString build   = tr("Build") + ": " + model::Version::LONG_STRING.toHtmlEscaped();

                // The fork line and the attribution are not passed through tr() as a whole: they name
                // the upstream project, which the brand translator must not rewrite.
                QString forkLine = model::Branding::ATTRIBUTION_SHORT.toHtmlEscaped() + " &middot; " + version +
                                   "<br/><span style='font-size:small;'>" + build + "</span>";

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

                QString productName = "<span style='font-size:20px; font-weight:700;'>" +
                                      model::Branding::DISPLAY_NAME.toHtmlEscaped() + "</span>";

                // Attribution is shown wherever the product name is shown.  It is deliberately NOT
                // passed through tr(): it names the upstream project and must not be rewritten.
                QString attribution =
                        model::Branding::ATTRIBUTION.toHtmlEscaped() +
                        " &mdash; <a href='" + model::Branding::UPSTREAM_URL + "'>" +
                        model::Branding::UPSTREAM_URL.toHtmlEscaped() + "</a>";

                // %1 is the upstream name, filled in after translation so that it is never rebranded.
                QString notAffiliated =
                        tr( "Licensed GPL-3.0-or-later. Not affiliated with or endorsed by the original %1 project." )
                        .arg( QStringLiteral( "gLabels" ) ).toHtmlEscaped();

                QString forkCopyright = "Copyright &copy; 2026 gLabels Neo contributors. This is a modified version of gLabels (see docs/CHANGES-FROM-UPSTREAM.md).";

                // Builds of the dev branch say so in the heading and carry a short warning.
                QString devNotice;
                if ( model::Version::IS_DEV_BUILD )
                {
                        productName += " <span style='font-size:13px; font-weight:700;'>(" + tr( "dev" ).toHtmlEscaped() + ")</span>";
                        devNotice = "<p align='center'><b>" + tr( "Development build" ).toHtmlEscaped() + "</b><br/>" +
                                    tr( "Made from the dev branch to try changes before they are released. It may be unstable and does not update itself." ).toHtmlEscaped() +
                                    "</p>";
                }

                QString markup =
                        "<p align='center'>" + productName + "</p>" +
                        "<p align='center'>" + forkLine + "</p>" +
                        devNotice +
                        "<p align='center'>" + description + "</p>" +
                        "<p align='center'>" + copyright + "<br/>" + forkCopyright + "</p>" +
                        "<p align='center'>" + attribution + "<br/>" + notAffiliated + "</p>" +
                        "<p align='left'>"   + licenseParagraph1 + "</p>" +
                        "<p align='left'>"   + licenseParagraph2 + "</p>";

                aboutLabel->setOpenExternalLinks( true );
                aboutLabel->setText( markup );

                // The text wraps; size the window for it so that nothing is cut off with the theme font.
                adjustSize();
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
