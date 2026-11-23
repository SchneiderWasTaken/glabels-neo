//  fake_batch.cpp
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

//
// Stands in for the new version of the program in the update tests.
//
//   --Version   answers the way glabels-batch-qt does (the updater's smoke test)
//   otherwise   records that it was started, and with which arguments, in started.txt next to itself
//

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

int main( int argc, char** argv )
{
        if ( argc >= 2 && std::strcmp( argv[1], "--Version" ) == 0 )
        {
                std::puts( "> GLABELS NEO" );
                return 0;
        }

        std::string args;
        for ( int i = 1; i < argc; i++ )
        {
                args += ( i > 1 ? "|" : "" );
                args += argv[i];
        }

        std::filesystem::path self = std::filesystem::absolute( argv[0] );
        std::ofstream( self.parent_path() / "started.txt" ) << args;
        return 0;
}
