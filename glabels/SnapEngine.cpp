//  SnapEngine.cpp
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


#include "SnapEngine.hpp"

#include <algorithm>
#include <cmath>


namespace glabels
{
        namespace
        {
                constexpr double kEps = 1e-9;

                struct AxisTarget
                {
                        double        pos{ 0.0 };
                        SnapGuideKind kind{ SnapGuideKind::Center };
                        QString       description;
                };

                struct AxisMatch
                {
                        bool      matched{ false };
                        double    delta{ 0.0 };
                        SnapGuide guide;
                };

                int kindPriority( SnapGuideKind kind )
                {
                        // SNAP.md: ties prefer Centre, then Safe area, then Objects
                        switch ( kind )
                        {
                        case SnapGuideKind::Center:       return 0;
                        case SnapGuideKind::SafeArea:     return 1;
                        case SnapGuideKind::ObjectEdge:   return 2;
                        case SnapGuideKind::ObjectCenter: return 3;
                        }
                        return 4;
                }

                void buildAxisTargets( const SnapContext&   ctx,
                                       QVector<AxisTarget>& xTargets,
                                       QVector<AxisTarget>& yTargets )
                {
                        const double w = ctx.labelSize.width();
                        const double h = ctx.labelSize.height();

                        if ( ctx.options.center && w > 0.0 && h > 0.0 )
                        {
                                const QString label = QStringLiteral( "Centre" );
                                xTargets.push_back( { w / 2.0, SnapGuideKind::Center, label } );
                                yTargets.push_back( { h / 2.0, SnapGuideKind::Center, label } );
                        }

                        if ( ctx.options.safeArea && w > 0.0 && h > 0.0 )
                        {
                                const double mx = std::clamp( ctx.safeInset.x, 0.0, w / 2.0 );
                                const double my = std::clamp( ctx.safeInset.y, 0.0, h / 2.0 );
                                const QString label = QStringLiteral( "Safe area" );

                                xTargets.push_back( { mx,     SnapGuideKind::SafeArea, label } );
                                xTargets.push_back( { w - mx, SnapGuideKind::SafeArea, label } );

                                yTargets.push_back( { my,     SnapGuideKind::SafeArea, label } );
                                yTargets.push_back( { h - my, SnapGuideKind::SafeArea, label } );
                        }

                        if ( ctx.options.objects )
                        {
                                for ( const SnapTargetObject& obj : ctx.targets )
                                {
                                        const QRectF b = obj.bounds.normalized();
                                        const QString type = obj.typeName.isEmpty()
                                                ? QStringLiteral( "object" )
                                                : obj.typeName;
                                        const QString edgeDesc   = QStringLiteral( "Edge of %1" ).arg( type );
                                        const QString centerDesc = QStringLiteral( "Centre of %1" ).arg( type );

                                        xTargets.push_back( { b.left(),       SnapGuideKind::ObjectEdge,   edgeDesc } );
                                        xTargets.push_back( { b.center().x(), SnapGuideKind::ObjectCenter, centerDesc } );
                                        xTargets.push_back( { b.right(),      SnapGuideKind::ObjectEdge,   edgeDesc } );

                                        yTargets.push_back( { b.top(),        SnapGuideKind::ObjectEdge,   edgeDesc } );
                                        yTargets.push_back( { b.center().y(), SnapGuideKind::ObjectCenter, centerDesc } );
                                        yTargets.push_back( { b.bottom(),     SnapGuideKind::ObjectEdge,   edgeDesc } );
                                }
                        }
                }

                AxisMatch solveAxis( const QVector<double>&     sources,
                                     const QVector<AxisTarget>& targets,
                                     double                     tolerance,
                                     SnapGuideOrientation       orientation )
                {
                        AxisMatch best;
                        if ( sources.isEmpty() || targets.isEmpty() || tolerance < 0.0 )
                        {
                                return best;
                        }

                        double bestDist = 0.0;
                        int    bestPrio = 999;

                        for ( double src : sources )
                        {
                                for ( const AxisTarget& tgt : targets )
                                {
                                        const double delta = tgt.pos - src;
                                        const double dist  = std::abs( delta );
                                        if ( dist > tolerance + kEps )
                                        {
                                                continue;
                                        }

                                        const int prio = kindPriority( tgt.kind );
                                        const bool closer = !best.matched || ( dist < bestDist - kEps );
                                        const bool tieWins = best.matched
                                                && ( std::abs( dist - bestDist ) <= kEps )
                                                && ( prio < bestPrio );

                                        if ( closer || tieWins )
                                        {
                                                best.matched = true;
                                                best.delta   = delta;
                                                best.guide   = SnapGuide{
                                                        orientation,
                                                        tgt.pos,
                                                        tgt.kind,
                                                        tgt.description
                                                };
                                                bestDist = dist;
                                                bestPrio = prio;
                                        }
                                }
                        }

                        return best;
                }

                SnapResult solveBoth( const QVector<double>& xSources,
                                      const QVector<double>& ySources,
                                      const SnapContext&     ctx )
                {
                        SnapResult res;
                        if ( !ctx.options.anyEnabled() || ctx.tolerance < 0.0 )
                        {
                                return res;
                        }

                        QVector<AxisTarget> xTargets;
                        QVector<AxisTarget> yTargets;
                        buildAxisTargets( ctx, xTargets, yTargets );

                        const AxisMatch mx = solveAxis( xSources, xTargets, ctx.tolerance, SnapGuideOrientation::Vertical );
                        if ( mx.matched )
                        {
                                res.dx       = mx.delta;
                                res.snappedX = true;
                                res.guides.push_back( mx.guide );
                        }

                        const AxisMatch my = solveAxis( ySources, yTargets, ctx.tolerance, SnapGuideOrientation::Horizontal );
                        if ( my.matched )
                        {
                                res.dy       = my.delta;
                                res.snappedY = true;
                                res.guides.push_back( my.guide );
                        }

                        return res;
                }
        } // namespace


        double SnapEngine::toleranceForScale( double scale, double screenPx )
        {
                if ( scale <= 0.0 || screenPx <= 0.0 )
                {
                        return 0.0;
                }
                return screenPx / scale;
        }


        SnapResult SnapEngine::snapMove( const QRectF& movingRect, const SnapContext& ctx )
        {
                const QRectF r = movingRect.normalized();
                const QVector<double> xSources{ r.left(), r.center().x(), r.right() };
                const QVector<double> ySources{ r.top(),  r.center().y(), r.bottom() };
                return solveBoth( xSources, ySources, ctx );
        }


        SnapResult SnapEngine::snapResize( const QRectF&      rect,
                                           SnapEdges          draggedEdges,
                                           const SnapContext& ctx )
        {
                const QRectF r = rect.normalized();
                QVector<double> xSources;
                QVector<double> ySources;

                if ( draggedEdges.testFlag( SnapEdge::Left ) )
                {
                        xSources.push_back( r.left() );
                }
                if ( draggedEdges.testFlag( SnapEdge::Right ) )
                {
                        xSources.push_back( r.right() );
                }
                if ( draggedEdges.testFlag( SnapEdge::Top ) )
                {
                        ySources.push_back( r.top() );
                }
                if ( draggedEdges.testFlag( SnapEdge::Bottom ) )
                {
                        ySources.push_back( r.bottom() );
                }

                return solveBoth( xSources, ySources, ctx );
        }


        SnapResult SnapEngine::snapPoint( const QPointF& point, const SnapContext& ctx )
        {
                return solveBoth( { point.x() }, { point.y() }, ctx );
        }


        SnapResult SnapEngine::snapCenter( const QRectF& rect, const SnapContext& ctx )
        {
                return snapPoint( rect.normalized().center(), ctx );
        }


        QRectF SnapEngine::rotatedBoundingBox( const QRectF&     localRect,
                                               const QTransform& matrix,
                                               const QPointF&    origin )
        {
                const QPointF pts[4] = {
                        matrix.map( localRect.topLeft() )     + origin,
                        matrix.map( localRect.topRight() )    + origin,
                        matrix.map( localRect.bottomRight() ) + origin,
                        matrix.map( localRect.bottomLeft() )  + origin
                };

                double minX = pts[0].x();
                double maxX = pts[0].x();
                double minY = pts[0].y();
                double maxY = pts[0].y();
                for ( int i = 1; i < 4; ++i )
                {
                        minX = std::min( minX, pts[i].x() );
                        maxX = std::max( maxX, pts[i].x() );
                        minY = std::min( minY, pts[i].y() );
                        maxY = std::max( maxY, pts[i].y() );
                }

                return QRectF( minX, minY, maxX - minX, maxY - minY );
        }


        QRectF SnapEngine::lineBoundingBox( const QPointF&    p1Local,
                                            const QPointF&    p2Local,
                                            const QTransform& matrix,
                                            const QPointF&    origin )
        {
                const QPointF a = matrix.map( p1Local ) + origin;
                const QPointF b = matrix.map( p2Local ) + origin;

                const double minX = std::min( a.x(), b.x() );
                const double maxX = std::max( a.x(), b.x() );
                const double minY = std::min( a.y(), b.y() );
                const double maxY = std::max( a.y(), b.y() );

                return QRectF( minX, minY, maxX - minX, maxY - minY );
        }

} // namespace glabels
