//  SnapEngine.hpp
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

#ifndef SnapEngine_hpp
#define SnapEngine_hpp


#include <QFlags>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTransform>
#include <QVector>


namespace glabels
{

        enum class SnapGuideOrientation
        {
                Vertical,
                Horizontal
        };

        enum class SnapGuideKind
        {
                Center,
                SafeArea,
                ObjectEdge,
                ObjectCenter
        };

        enum class SnapEdge : unsigned
        {
                None   = 0,
                Left   = 1u << 0,
                Right  = 1u << 1,
                Top    = 1u << 2,
                Bottom = 1u << 3
        };
        Q_DECLARE_FLAGS( SnapEdges, SnapEdge )

        struct SnapOptions
        {
                bool objects{ true };
                bool center{ true };
                bool safeArea{ true };

                bool anyEnabled() const
                {
                        return objects || center || safeArea;
                }
        };

        struct SnapInset
        {
                double x{ 0.0 };
                double y{ 0.0 };
        };

        struct SnapTargetObject
        {
                QRectF  bounds;
                QString typeName;
        };

        struct SnapGuide
        {
                SnapGuideOrientation orientation{ SnapGuideOrientation::Vertical };
                double               position{ 0.0 };
                SnapGuideKind        kind{ SnapGuideKind::Center };
                QString              description;
        };

        struct SnapResult
        {
                double             dx{ 0.0 };
                double             dy{ 0.0 };
                bool               snappedX{ false };
                bool               snappedY{ false };
                QVector<SnapGuide> guides;

                bool hasSafeAreaSnap() const
                {
                        for ( const SnapGuide& g : guides )
                        {
                                if ( g.kind == SnapGuideKind::SafeArea )
                                {
                                        return true;
                                }
                        }
                        return false;
                }
        };

        struct SnapContext
        {
                QSizeF                    labelSize;
                SnapInset                 safeInset;
                QVector<SnapTargetObject> targets;
                SnapOptions               options;
                double                    tolerance{ 0.0 };
        };


        ///
        /// Display-free snapping and guide computation (docs/ribbon-ui/SNAP.md).
        /// All coordinates and tolerances are in label units (points).
        ///
        class SnapEngine
        {
        public:
                static constexpr double DefaultScreenTolerancePx = 6.0;

                static double toleranceForScale( double scale,
                                                 double screenPx = DefaultScreenTolerancePx );

                static SnapResult snapMove( const QRectF&      movingRect,
                                            const SnapContext& ctx );

                static SnapResult snapResize( const QRectF&      rect,
                                              SnapEdges          draggedEdges,
                                              const SnapContext& ctx );

                static SnapResult snapPoint( const QPointF&     point,
                                             const SnapContext& ctx );

                static SnapResult snapCenter( const QRectF&      rect,
                                              const SnapContext& ctx );

                static QRectF rotatedBoundingBox( const QRectF&     localRect,
                                                  const QTransform& matrix,
                                                  const QPointF&    origin );

                static QRectF lineBoundingBox( const QPointF&    p1Local,
                                               const QPointF&    p2Local,
                                               const QTransform& matrix,
                                               const QPointF&    origin );
        };

} // namespace glabels

Q_DECLARE_OPERATORS_FOR_FLAGS( glabels::SnapEdges )


#endif // SnapEngine_hpp
