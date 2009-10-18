/*
 * Stellarium
 * Copyright (C) 2007 Guillaume Chereau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <QDebug>
#include <QBuffer>
#include <stdexcept>

#include "StelSphereGeometry.hpp"
#include "StelUtils.hpp"
#include "StelJsonParser.hpp"

#include "glues.h"

// Definition of static constants.
const QVariant::Type SphericalRegionP::qVariantType = (QVariant::Type)(QVariant::UserType+1);
int SphericalRegionP::metaTypeId = SphericalRegionP::initialize();

int SphericalRegionP::initialize()
{
	int id = SphericalRegionP::metaTypeId = qRegisterMetaType<SphericalRegionP>();
	qRegisterMetaTypeStreamOperators<SphericalRegionP>("SphericalRegionP");
	return id;
}

SphericalRegionP SphericalRegionP::getIntersection(const SphericalRegionP& reg1, const SphericalRegionP& reg2)
{
	if (reg1->getType()==SphericalRegion::AllSky)
	{
		if (reg2->getType()==SphericalRegion::AllSky)
			return SphericalRegionP(new AllSkySphericalRegion());
		return SphericalRegionP(new SphericalPolygon(reg2->toSphericalPolygon()));
	}
	if (reg2->getType()==SphericalRegion::AllSky)
		return SphericalRegionP(new SphericalPolygon(reg1->toSphericalPolygon()));

	return SphericalRegionP(new SphericalPolygon(reg1->toSphericalPolygon().getIntersection(reg2->toSphericalPolygon())));
}

SphericalRegionP SphericalRegionP::getUnion(const SphericalRegionP& reg1, const SphericalRegionP& reg2)
{
	if (reg1->getType()==SphericalRegion::AllSky || reg2->getType()!=SphericalRegion::AllSky)
	{
		return SphericalRegionP(new AllSkySphericalRegion());
	}
	return SphericalRegionP(new SphericalPolygon(reg1->toSphericalPolygon().getUnion(reg2->toSphericalPolygon())));
}

SphericalRegionP SphericalRegionP::getSubtraction(const SphericalRegionP& reg1, const SphericalRegionP& reg2)
{
	return SphericalRegionP(new SphericalPolygon(reg1->toSphericalPolygon().getSubtraction(reg2->toSphericalPolygon())));
}

QDataStream& operator<<(QDataStream& out, const SphericalRegionP& region)
{
	out << region->toQVariant();
	return out;
}

QDataStream& operator>>(QDataStream& in, SphericalRegionP& region)
{
	QVariantMap v;
	in >> v;
	try
	{
		region=SphericalRegion::loadFromQVariant(v);
	}
	catch (std::runtime_error& e)
	{
		qWarning() << e.what();
		Q_ASSERT(0);
	}
	return in;
}

bool SphericalRegion::contains(const SphericalRegionP& region) const
{
	switch (region->getType())
	{
		case SphericalRegion::Point:
			return contains(static_cast<const SphericalPoint*>(region.data())->n);
		case SphericalRegion::Cap:
			return contains(*static_cast<const SphericalCap*>(region.data()));
		case SphericalRegion::Polygon:
		case SphericalRegion::ConvexPolygon:
			return contains(*static_cast<const SphericalPolygonBase*>(region.data()));
		case SphericalRegion::AllSky:
			return contains(*static_cast<const AllSkySphericalRegion*>(region.data()));
		case SphericalRegion::Empty:
			return false;
	}
	Q_ASSERT(0);
	return false;
}

bool SphericalRegion::intersects(const SphericalRegionP& region) const
{
	switch (region->getType())
	{
		case SphericalRegion::Point:
			return contains(static_cast<const SphericalPoint*>(region.data())->n);
		case SphericalRegion::Cap:
			return intersects(*static_cast<const SphericalCap*>(region.data()));
		case SphericalRegion::Polygon:
		case SphericalRegion::ConvexPolygon:
			return intersects(*static_cast<const SphericalPolygonBase*>(region.data()));
		case SphericalRegion::AllSky:
			return intersects(*static_cast<const AllSkySphericalRegion*>(region.data()));
		case SphericalRegion::Empty:
			return false;
	}
	Q_ASSERT(0);
	return false;
}

QByteArray SphericalRegion::toJSON() const
{
	QByteArray res;
	QBuffer buf1(&res);
	buf1.open(QIODevice::WriteOnly);
	StelJsonParser::write(toQVariant(), buf1);
	buf1.close();
	return res;
}

bool SphericalPoint::intersects(const SphericalPolygonBase& mpoly) const
{
	const SphericalConvexPolygon* cvx = dynamic_cast<const SphericalConvexPolygon*>(&mpoly);
	if (cvx!=NULL)
		return cvx->contains(n);
	else
		return static_cast<const SphericalPolygon*>(&mpoly)->contains(n);
}

SphericalPolygon SphericalPoint::toSphericalPolygon() const
{
	QVector<Vec3d> contour;
	contour << n << n << n;
	return SphericalPolygon(contour);
}

SphericalRegionP SphericalRegion::getEnlarged(double margin) const
{
	Q_ASSERT(margin>=0);
	if (margin>=M_PI)
		return SphericalRegionP(new AllSkySphericalRegion());
	const SphericalCap& cap = getBoundingCap();
	double newRadius = std::acos(cap.d)+margin;
	if (newRadius>=M_PI)
		return SphericalRegionP(new AllSkySphericalRegion());
	return SphericalRegionP(new SphericalCap(cap.n, std::cos(newRadius)));
}

SphericalPolygon SphericalCap::toSphericalPolygon() const
{
	return toSphericalConvexPolygon().toSphericalPolygon();
}

// Returns whether a SphericalPolygon is contained into the region.
bool SphericalCap::contains(const SphericalPolygonBase& polyBase) const
{
	const SphericalConvexPolygon* cvx = dynamic_cast<const SphericalConvexPolygon*>(&polyBase);
	if (cvx!=NULL)
	{
		foreach (const Vec3d& v, cvx->getConvexContour())
		{
			if (!contains(v))
				return false;
		}
		return true;
	}
	Q_ASSERT(0); // Not implemented
	return false;
}

bool SphericalCap::intersectsConvexContour(const Vec3d* vertice, int nbVertice) const
{
	for (int i=0;i<nbVertice;++i)
	{
		if (contains(vertice[i]))
			return true;
	}
	// No points of the convex polygon are inside the cap
	if (d<=0)
		return false;

	for (int i=0;i<nbVertice-1;++i)
	{
		if (!sideHalfSpaceIntersects(vertice[i], vertice[i+1], *this))
			return false;
	}
	if (!sideHalfSpaceIntersects(vertice[nbVertice-1], vertice[0], *this))
		return false;

	// Warning!!!! There is a last case which is not managed!
	// When all the points of the polygon are outside the circle but the halfspace of the corner the closest to the
	// circle intersects the circle halfspace..
	return true;
}

// Returns whether a SphericalPolygon intersects the region.
bool SphericalCap::intersects(const SphericalPolygonBase& polyBase) const
{
	// TODO This algo returns sometimes false positives!!
	const SphericalConvexPolygon* cvx = dynamic_cast<const SphericalConvexPolygon*>(&polyBase);
	if (cvx!=NULL)
		return intersectsConvexContour(cvx->getConvexContour().constData(), cvx->getConvexContour().size());
	// Go through the full list of triangle
	const QVector<Vec3d>& vArray = polyBase.getVertexArray().vertex;
	for (int i=0;i<vArray.size()/3;++i)
	{
		if (intersectsConvexContour(vArray.constData()+i*3, 3))
			return true;
	}
	return false;
}

// Convert the cap into a SphericalRegionBase instance.
SphericalConvexPolygon SphericalCap::toSphericalConvexPolygon() const
{
	static const int nbStep = 40;
	QVector<Vec3d> contour;
	Vec3d p(n);
	Vec3d axis = n^Vec3d(1,0,0);
	if (axis.lengthSquared()<0.1)
		axis = n^Vec3d(0,1,0);	// Improve precision
	p.transfo4d(Mat4d::rotation(axis, std::acos(d)));
	const Mat4d& rot = Mat4d::rotation(n, -2.*M_PI/nbStep);
	for (int i=0;i<nbStep;++i)
	{
		contour.append(p);
		p.transfo4d(rot);
	}
	return SphericalConvexPolygon(contour);
}

SphericalPolygon AllSkySphericalRegion::toSphericalPolygon() const
{
	Q_ASSERT(0); return SphericalPolygon();
}

///////////////////////////////////////////////////////////////////////////////
// Methods for SphericalPolygonBase
///////////////////////////////////////////////////////////////////////////////

struct UserDataSimplifiedContours
{
	// Contour used by the tesselator as temporary objects
	QList<Vec3d> tmpVectors;
	// Contour used by the tesselator as temporary objects
	QVector<QVector<Vec3d> > resultContours;
};

void  errorCallback(GLenum errno)
{
	qWarning() << "Tesselator error:" << QString::fromAscii((char*)gluesErrorString(errno));
	Q_ASSERT(0);
}

void  contourBeginCallback(GLenum type, void* userData)
{
	Q_ASSERT(type==GL_LINE_LOOP);
	UserDataSimplifiedContours* d = static_cast<UserDataSimplifiedContours*>(userData);
	d->resultContours.append(QVector<Vec3d>());
}

void  contourVertexCallback(void* vertexData, void* userData)
{
	const double* v = (double*)vertexData;
	UserDataSimplifiedContours* d = static_cast<UserDataSimplifiedContours*>(userData);
	d->resultContours.last().append(Vec3d(v[0], v[1], v[2]));
}

void  combineCallbackSimple(double coords[3], void* vertex_data[4], GLfloat weight[4], void** outData, void* userData)
{
	UserDataSimplifiedContours* d = static_cast<UserDataSimplifiedContours*>(userData);
	d->tmpVectors.append(Vec3d(coords[0], coords[1], coords[2]));
	d->tmpVectors.last().normalize();
	*outData = d->tmpVectors.last();
}

QVector<QVector<Vec3d> > SphericalPolygonBase::getSimplifiedContours() const
{
	// Use GLUES tesselation functions to compute the contours from the list of triangles
	GLUEStesselator* tess = gluesNewTess();
	gluesTessCallback(tess, GLUES_TESS_BEGIN_DATA, (GLvoid(*)()) &contourBeginCallback);
	gluesTessCallback(tess, GLUES_TESS_VERTEX_DATA, (GLvoid(*)()) &contourVertexCallback);
	gluesTessCallback(tess, GLUES_TESS_ERROR, (GLvoid (*) ()) &errorCallback);
	gluesTessProperty(tess, GLUES_TESS_WINDING_RULE, GLUES_TESS_WINDING_POSITIVE);
	gluesTessProperty(tess, GLUES_TESS_BOUNDARY_ONLY, GL_TRUE);
	gluesTessCallback(tess, GLUES_TESS_COMBINE_DATA, (GLvoid (*)()) &combineCallbackSimple);
	//gluesTessProperty(tess, GLUES_TESS_TOLERANCE, 0.00000001);

	UserDataSimplifiedContours userData;
	gluesTessBeginPolygon(tess, &userData);
	const QVector<Vec3d>& trianglesArray = getVertexArray().vertex;
	for (int c=0;c<trianglesArray.size()/3;++c)
	{
		gluesTessBeginContour(tess);
                gluesTessVertex(tess, const_cast<double*>((const double*)trianglesArray.at(c*3)), const_cast<double*>((const double*)trianglesArray.at(c*3)));
                gluesTessVertex(tess, const_cast<double*>((const double*)trianglesArray.at(c*3+1)), const_cast<double*>((const double*)trianglesArray.at(c*3+1)));
                gluesTessVertex(tess, const_cast<double*>((const double*)trianglesArray.at(c*3+2)), const_cast<double*>((const double*)trianglesArray.at(c*3+2)));
		gluesTessEndContour(tess);
	}
	gluesTessEndPolygon(tess);
	gluesDeleteTess(tess);
#ifndef NDEBUG
	// Check that all vectors are normalized
	foreach (const QVector<Vec3d> c, userData.resultContours)
		foreach (const Vec3d& v, c)
			Q_ASSERT(std::fabs(v.lengthSquared()-1.)<0.000001);
#endif
	return userData.resultContours;
}

QVector<QVector<Vec3d> > SphericalPolygon::getContours() const
{
	Q_ASSERT(triangleVertices.size()%3==0);
	QVector<QVector<Vec3d> > res;
	for (int i=0;i<triangleVertices.size()/3;++i)
	{
		res+=triangleVertices.mid(i*3, 3);
	}
#ifndef NDEBUG
	foreach (const QVector<Vec3d>& l, res)
	{
		Q_ASSERT((l.at(1)^l.at(0))*l.at(2)>=0);
	}
#endif
	return res;
}

// Returns whether another SphericalPolygon intersects with the SphericalPolygon.
bool SphericalPolygonBase::intersects(const SphericalPolygonBase& mpoly) const
{
	if (!getBoundingCap().intersects(mpoly.getBoundingCap()))
		return false;
	return !getIntersection(mpoly).getVertexArray().vertex.isEmpty();
}

// Return a new SphericalPolygon consisting of the intersection of this and the given SphericalPolygon.
SphericalPolygon SphericalPolygonBase::getIntersection(const SphericalPolygonBase& mpoly) const
{
	if (!getBoundingCap().intersects(mpoly.getBoundingCap()))
		return SphericalPolygon();
	QVector<QVector<Vec3d> > allContours = getSimplifiedContours();
	allContours += mpoly.getSimplifiedContours();
	SphericalPolygon p;
	p.setContours(allContours, SphericalPolygonBase::WindingAbsGeqTwo);
	return p;
}

// Return a new SphericalPolygon consisting of the union of this and the given SphericalPolygon.
SphericalPolygon SphericalPolygonBase::getUnion(const SphericalPolygonBase& mpoly) const
{
	QVector<QVector<Vec3d> > allContours = getContours();
	allContours += mpoly.getContours();
	return SphericalPolygon(allContours);
}

// Return a new SphericalPolygon consisting of the subtraction of the given SphericalPolygon from this.
SphericalPolygon SphericalPolygonBase::getSubtraction(const SphericalPolygonBase& mpoly) const
{
	QVector<QVector<Vec3d> > allContours = getContours();
	foreach (const QVector<Vec3d>& c, mpoly.getContours())
	{
		QVector<Vec3d> cr;
		cr.reserve(c.size());
		for (int i=c.size()-1;i>=0;--i)
			cr.append(c.at(i));
		allContours.append(cr);
	}
	return SphericalPolygon(allContours);
}

// Return the area in squared degrees.
double SphericalPolygonBase::getArea() const
{
	// Use Girard's theorem for each subtriangles
	double area = 0.;
	Vec3d v1, v2, v3;
	const QVector<Vec3d>& trianglesArray = getVertexArray().vertex;
	for (int i=0;i<trianglesArray.size()/3;++i)
	{
		v1 = trianglesArray[i*3+0] ^ trianglesArray[i*3+1];
		v2 = trianglesArray[i*3+1] ^ trianglesArray[i*3+2];
		v3 = trianglesArray[i*3+2] ^ trianglesArray[i*3+0];
		area += 2.*M_PI - v1.angle(v2) - v2.angle(v3) - v3.angle(v1);
	}
	return area;
}

// Return a point located inside the polygon.
Vec3d SphericalPolygonBase::getPointInside() const
{
	const QVector<Vec3d>& trianglesArray = getVertexArray().vertex;
	Vec3d res = trianglesArray[0]+trianglesArray[1]+trianglesArray[2];
	res.normalize();
	return res;
}

// Default slow implementation o(n^2).
SphericalCap SphericalPolygonBase::getBoundingCap() const
{
	Vec3d p1(1,0,0), p2(1,0,0);
	double maxDist=1.;
	const QVector<Vec3d>& trianglesArray = getVertexArray().vertex;
	foreach (const Vec3d& v1, trianglesArray)
	{
		foreach (const Vec3d& v2, trianglesArray)
		{
			if (v1*v2<maxDist)
			{
				p1 = v1;
				p2 = v2;
				maxDist = v1*v2;
			}
		}
	}
	Vec3d res = p1+p2;
	res.normalize();
	return SphericalCap(res, res*p1);
}

///////////////////////////////////////////////////////////////////////////////
// Methods for SphericalPolygon
///////////////////////////////////////////////////////////////////////////////

// Store data for the GLUES tesselation callbacks
struct GluTessCallbackData
{
	SphericalPolygon* thisPolygon;	//! Reference to the instance of SphericalPolygon being tesselated.
	bool edgeFlag;					//! Used to store temporary edgeFlag found by the tesselator.
	QList<Vec3d> tempVertices;	//! Use to contain the temporary combined vertices
};

void  vertexCallback(void* vertexData, void* userData)
{
	SphericalPolygon* mp = ((GluTessCallbackData*)userData)->thisPolygon;
	const double* v = (double*)vertexData;
	Vec3d vv(v[0], v[1], v[2]);
	Q_ASSERT(std::fabs(vv.length()-1.)<0.000001);
	mp->triangleVertices.append(vv);
	mp->edgeFlags.append(((GluTessCallbackData*)userData)->edgeFlag);
}

void  edgeFlagCallback(GLboolean flag, void* userData)
{
	((GluTessCallbackData*)userData)->edgeFlag=flag;
}

void  combineCallback(double coords[3], void* vertex_data[4], GLfloat weight[4], void** outData, void* userData)
{
	QList<Vec3d>& tempVertices = ((GluTessCallbackData*)userData)->tempVertices;
	if (vertex_data[2]==NULL)
	{
		// Only 2 vertices to combine: easy
		double* dd = (double*)vertex_data[0];
		Vec3d newVertex(dd[0]*weight[0],dd[1]*weight[0],dd[2]*weight[0]);
		dd = (double*)vertex_data[1];
		newVertex[0]+=dd[0]*weight[1];
		newVertex[1]+=dd[1]*weight[1];
		newVertex[2]+=dd[2]*weight[1];
		newVertex.normalize();
		tempVertices.append(newVertex);
	}
// 	else if (vertex_data[3]!=NULL)
// 	{
// 		// 4 vertices are given, compute the intersection of the 2 great circles
// 		double* dd = (double*)vertex_data[0];
// 		Vec3d v0(dd[0],dd[1],dd[2]);
// 		dd = (double*)vertex_data[1];
// 		const Vec3d v1(dd[0],dd[1],dd[2]);
// 		dd = (double*)vertex_data[2];
// 		const Vec3d v2(dd[0],dd[1],dd[2]);
// 		dd = (double*)vertex_data[3];
// 		const Vec3d v3(dd[0],dd[1],dd[2]);
// 		bool ok;
// 		v0 = greatCircleIntersection(v0, v1, v2, v3, ok);
// 		if (!ok)
// 		{
// 			// The 2 great circles are identical, in this case return the closest point
// 			const int i0= (weight[0]>weight[1]) ? 0 : 1;
// 			const int i1= (weight[2]>weight[3]) ? 2 : 3;
// 			const int i = (weight[i0]>weight[i1]) ? i0 : i1;
// 			dd = (double*)vertex_data[i];
// 			v0.set(dd[0],dd[1],dd[2]);
// 		}
// 		tempVertices.append(v0);
// 	}
	else
	{
		// 3 vertices: unsupported case..
//		Q_ASSERT(0);
		tempVertices.append(Vec3d(coords[0], coords[1], coords[2]));
		tempVertices.last().normalize();
	}
	*outData = tempVertices.last();
}

void  checkBeginCallback(GLenum type)
{
	Q_ASSERT(type==GL_TRIANGLES);
}

void SphericalPolygon::setContours(const QVector<QVector<Vec3d> >& contours, SphericalPolygonBase::PolyWindingRule windingRule)
{
	triangleVertices.clear();
	edgeFlags.clear();

	// Use GLUES tesselation functions to transform the polygon into a list of triangles
	GLUEStesselator* tess = gluesNewTess();
#ifndef NDEBUG
	gluesTessCallback(tess, GLUES_TESS_BEGIN, (GLvoid(*)()) &checkBeginCallback);
#endif
	gluesTessCallback(tess, GLUES_TESS_VERTEX_DATA, (GLvoid(*)()) &vertexCallback);
	gluesTessCallback(tess, GLUES_TESS_EDGE_FLAG_DATA, (GLvoid(*)()) &edgeFlagCallback);
	gluesTessCallback(tess, GLUES_TESS_ERROR, (GLvoid(*)()) &errorCallback);
	gluesTessCallback(tess, GLUES_TESS_COMBINE_DATA, (GLvoid(*)()) &combineCallback);
        const double windRule = (windingRule==SphericalPolygonBase::WindingPositive) ? GLUES_TESS_WINDING_POSITIVE : GLUES_TESS_WINDING_ABS_GEQ_TWO;
	gluesTessProperty(tess, GLUES_TESS_WINDING_RULE, windRule);
	//gluesTessProperty(tess, GLUES_TESS_TOLERANCE, 0.00001);
	GluTessCallbackData data;
	data.thisPolygon=this;
	gluesTessBeginPolygon(tess, &data);
	for (int c=0;c<contours.size();++c)
	{
		gluesTessBeginContour(tess);
		foreach (const Vec3d& v, contours.at(c))
		{
                        gluesTessVertex(tess, const_cast<double*>((const double*)v), const_cast<void*>((const void*)v));
		}
		gluesTessEndContour(tess);
	}
	gluesTessEndPolygon(tess);
	gluesDeleteTess(tess);

	// There should always be an edge flag matching each vertex.
	Q_ASSERT(triangleVertices.size() == edgeFlags.size());
	Q_ASSERT(triangleVertices.size()%3==0);
#ifndef NDEBUG
	// Check that all vectors are normalized
	foreach (const Vec3d& v, triangleVertices)
		Q_ASSERT(std::fabs(v.lengthSquared()-1.)<0.000001);

	// Check that the orientation of all the triangles is positive
	for (int i=0;i<triangleVertices.size()/3;++i)
	{
		if(!((triangleVertices.at(i*3+1)^triangleVertices.at(i*3))*triangleVertices.at(i*3+2)>=0))
		{
			// Avoid crash but very bad!
			qWarning() << "Warning, couldn't tesselate a polygon";
			triangleVertices.clear();
			edgeFlags.clear();
			return;

// 			triangleVertices.remove(i*3, 3);
// 			edgeFlags.remove(i*3, 3);
			static int k=0;
			qDebug() << ++k;
			for (int i=0;i<contours.size();++i)
			{
				QString contourStr = "[";
				foreach (const Vec3d& v, contours[i])
				{
					contourStr += QString("[%1, %2]").arg(v.longitude()*180./M_PI, 0, 'g', 15).arg(v.latitude()*180./M_PI, 0, 'g', 15);
				}
				contourStr += "]";
				qDebug() << QString("Contour %1: %2").arg(i).arg(contourStr);
			}

			qDebug() << triangleVertices.size()/3;
			for (int i=0;i<triangleVertices.size()/3;++i)
			{
				QString contourStr = "[";
				for (int j=i*3;j<i*3+3;++j)
				{
					Vec3d v = triangleVertices.at(j);
					contourStr += QString("[%1, %2]").arg(v.longitude()*180./M_PI, 0, 'g', 15).arg(v.latitude()*180./M_PI, 0, 'g', 15);
				}
				contourStr += "]";
				qDebug() << QString("Triangle %1: %2").arg(i).arg(contourStr);
			}
			Q_ASSERT(0);
		}
	}
#endif
}

// Set a single contour defining the SphericalPolygon.
void SphericalPolygon::setContour(const QVector<Vec3d>& contour)
{
	QVector<QVector<Vec3d> > contours;
	contours.append(contour);
	setContours(contours);
}



///////////////////////////////////////////////////////////////////////////////
// Methods for SphericalTexturedPolygon
///////////////////////////////////////////////////////////////////////////////
void  vertexTextureCallback(void* vertexData, void* userData)
{
	SphericalTexturedPolygon* mp = static_cast<SphericalTexturedPolygon*>(((GluTessCallbackData*)userData)->thisPolygon);
	const SphericalTexturedPolygon::TextureVertex* vData = (SphericalTexturedPolygon::TextureVertex*)vertexData;
	mp->triangleVertices.append(vData->vertex);
	mp->textureCoords.append(vData->texCoord);
	mp->edgeFlags.append(((GluTessCallbackData*)userData)->edgeFlag);
}

void SphericalTexturedPolygon::setContours(const QVector<QVector<TextureVertex> >& contours, SphericalPolygonBase::PolyWindingRule windingRule)
{
	triangleVertices.clear();
	edgeFlags.clear();
	textureCoords.clear();

	// Use GLUES tesselation functions to transform the polygon into a list of triangles
	GLUEStesselator* tess = gluesNewTess();
	gluesTessCallback(tess, GLUES_TESS_VERTEX_DATA, (GLvoid(*)()) &vertexTextureCallback);
	gluesTessCallback(tess, GLUES_TESS_EDGE_FLAG_DATA, (GLvoid(*)()) &edgeFlagCallback);
	gluesTessCallback(tess, GLUES_TESS_ERROR, (GLvoid (*) ()) &errorCallback);
        const double windRule = (windingRule==SphericalPolygonBase::WindingPositive) ? GLUES_TESS_WINDING_POSITIVE : GLUES_TESS_WINDING_ABS_GEQ_TWO;
	gluesTessProperty(tess, GLUES_TESS_WINDING_RULE, windRule);
	GluTessCallbackData data;
	data.thisPolygon=this;
	gluesTessBeginPolygon(tess, &data);
	for (int c=0;c<contours.size();++c)
	{
		gluesTessBeginContour(tess);
		for (int i=0;i<contours[c].size();++i)
		{
                        gluesTessVertex(tess, const_cast<double*>((const double*)contours[c][i].vertex), const_cast<void*>((const void*)&(contours[c][i])));
		}
		gluesTessEndContour(tess);
	}
	gluesTessEndPolygon(tess);
	gluesDeleteTess(tess);

	// There should always be a texture coord matching each vertex.
	Q_ASSERT(triangleVertices.size() == edgeFlags.size());
	Q_ASSERT(triangleVertices.size() == textureCoords.size());

	// There should always be an edge flag matching each vertex.
	Q_ASSERT(triangleVertices.size() == edgeFlags.size());
#ifndef NDEBUG
	// Check that all vectors are normalized
	foreach (const Vec3d& v, triangleVertices)
		Q_ASSERT(std::fabs(v.lengthSquared()-1.)<0.000001);
	// Check that the orientation of all the triangles is positive
	for (int i=0;i<triangleVertices.size()/3;++i)
	{
		Q_ASSERT((triangleVertices.at(i*3+1)^triangleVertices.at(i*3))*triangleVertices.at(i*3+2)>=0);
	}
#endif
}

void SphericalTexturedPolygon::setContour(const QVector<TextureVertex>& contour)
{
	QVector<QVector<TextureVertex> > contours;
	contours.append(contour);
	setContours(contours, SphericalPolygonBase::WindingPositive);
}

// Returns whether a point is contained into the SphericalPolygon.
bool SphericalPolygon::contains(const Vec3d& p) const
{
	const QVector<Vec3d>& trianglesArray = getVertexArray().vertex;
	for (int i=0;i<trianglesArray.size()/3;++i)
	{
		if (sideHalfSpaceContains(trianglesArray.at(i*3+1), trianglesArray.at(i*3), p) &&
				  sideHalfSpaceContains(trianglesArray.at(i*3+2), trianglesArray.at(i*3+1), p) &&
				  sideHalfSpaceContains(trianglesArray.at(i*3+0), trianglesArray.at(i*3+2), p))
			return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// Methods for SphericalConvexPolygon
///////////////////////////////////////////////////////////////////////////////

// Return an openGL compatible array to be displayed using vertex arrays.
StelVertexArray SphericalConvexPolygon::getVertexArray() const
{
	StelVertexArray ar;
	ar.vertex << contour.at(0) << contour.at(1) << contour.at(2);
	ar.edgeFlags << true << true << false;
	for (int i=2;i<contour.size()-2;++i)
	{
		ar.vertex << contour.at(0) << contour.at(i) << contour.at(i+1);
		ar.edgeFlags << false << true << false;
	}
	// Last triangle
	ar.vertex << contour.at(0) << contour.at(contour.size()-2) << contour.last();
	ar.edgeFlags << false << true << true;
	return ar;
}

// Return an openGL compatible array to be displayed using vertex arrays.
StelVertexArray SphericalTexturedConvexPolygon::getVertexArray() const
{
	StelVertexArray ar;
	// Need to compute textures coordinates
	ar.vertex << contour.at(0) << contour.at(1) << contour.at(2);
	ar.edgeFlags << true << true << false;
	ar.texCoords << textureCoords.at(0) << textureCoords.at(1) << textureCoords.at(2);
	for (int i=2;i<contour.size()-2;++i)
	{
		ar.vertex << contour.at(0) << contour.at(i) << contour.at(i+1);
		ar.edgeFlags << false << true << false;
		ar.texCoords << textureCoords.at(0) << textureCoords.at(i) << textureCoords.at(i+1);
	}
	// Last triangle
	ar.vertex << contour.at(0) << contour.at(contour.size()-2) << contour.last();
	ar.edgeFlags << false << true << true;
	ar.texCoords << textureCoords.at(0) << textureCoords.at(contour.size()-2) << textureCoords.last();
	return ar;
}

// Check if the polygon is valid, i.e. it has no side >180
bool SphericalConvexPolygon::checkValid() const
{
	return SphericalConvexPolygon::checkValidContour(contour);
}

bool SphericalConvexPolygon::checkValidContour(const QVector<Vec3d>& contour)
{
	if (contour.size()<3)
		return false;
	bool res=true;
	for (int i=0;i<contour.size()-1;++i)
	{
		// Check that all points not on the current convex plane are included in it
		for (int p=0;p<contour.size()-2;++p)
			res &= sideHalfSpaceContains(contour.at(i+1), contour.at(i), contour[(p+i+2)%contour.size()]);
	}
	for (int p=0;p<contour.size()-2;++p)
		res &= sideHalfSpaceContains(contour.first(), contour.last(), contour[(p+contour.size()+1)%contour.size()]);
	return res;
}

// Return the list of halfspace bounding the ConvexPolygon.
QVector<SphericalCap> SphericalConvexPolygon::getBoundingSphericalCaps() const
{
	QVector<SphericalCap> res;
	for (int i=0;i<contour.size()-1;++i)
		res << SphericalCap(contour.at(i+1)^contour.at(i), 0);
	res << SphericalCap(contour.first()^contour.last(), 0);
	return res;
}

// Returns whether a point is contained into the region.
bool SphericalConvexPolygon::contains(const Vec3d& p) const
{
	for (int i=0;i<contour.size()-1;++i)
	{
		if (!sideHalfSpaceContains(contour.at(i+1), contour.at(i), p))
			return false;
	}
	return sideHalfSpaceContains(contour.first(), contour.last(), p);
}

bool SphericalConvexPolygon::contains(const SphericalCap& c) const
{
	for (int i=0;i<contour.size()-1;++i)
	{
		if (!sideHalfSpaceContains(contour.at(i+1), contour.at(i), c))
			return false;
	}
	return sideHalfSpaceContains(contour.first(), contour.last(), c);
}

bool SphericalConvexPolygon::containsConvexContour(const Vec3d* vertice, int nbVertex) const
{
	for (int i=0;i<nbVertex;++i)
	{
		if (!contains(vertice[i]))
			return false;
	}
	return true;
}

// Returns whether a SphericalPolygon is contained into the region.
bool SphericalConvexPolygon::contains(const SphericalPolygonBase& polyBase) const
{
	const SphericalConvexPolygon* cvx = dynamic_cast<const SphericalConvexPolygon*>(&polyBase);
	if (cvx!=NULL)
	{
		return containsConvexContour(cvx->getConvexContour().constData(), cvx->getConvexContour().size());
	}
	// For standard polygons, go through the full list of triangles
	const QVector<Vec3d>& vArray = polyBase.getVertexArray().vertex;
	for (int i=0;i<vArray.size()/3;++i)
	{
		if (!containsConvexContour(vArray.constData()+i*3, 3))
			return false;
	}
	return true;
}

bool SphericalConvexPolygon::areAllPointsOutsideOneSide(const Vec3d* thisContour, int nbThisContour, const Vec3d* points, int nbPoints)
{
	for (int i=0;i<nbThisContour-1;++i)
	{
		bool allOutside = true;
		for (int j=0;j<nbPoints&& allOutside==true;++j)
		{
			allOutside = allOutside && !sideHalfSpaceContains(thisContour[i+1], thisContour[i], points[j]);
		}
		if (allOutside)
			return true;
	}

		// Last iteration
	bool allOutside = true;
	for (int j=0;j<nbPoints&& allOutside==true;++j)
	{
		allOutside = allOutside && !sideHalfSpaceContains(thisContour[0], thisContour[nbThisContour-1], points[j]);
	}
	if (allOutside)
		return true;

		// Else
	return false;
}

// Returns whether another SphericalPolygon intersects with the SphericalPolygon.
bool SphericalConvexPolygon::intersects(const SphericalPolygonBase& polyBase) const
{
	const SphericalConvexPolygon* cvx = dynamic_cast<const SphericalConvexPolygon*>(&polyBase);
	if (cvx!=NULL)
	{
		return !areAllPointsOutsideOneSide(cvx->contour) && !cvx->areAllPointsOutsideOneSide(contour);
	}
	// For standard polygons, go through the full list of triangles
	const QVector<Vec3d>& vArray = polyBase.getVertexArray().vertex;
	for (int i=0;i<vArray.size()/3;++i)
	{
		if (!areAllPointsOutsideOneSide(contour.constData(), contour.size(), vArray.constData()+i*3, 3) && !cvx->areAllPointsOutsideOneSide(vArray.constData()+i*3, 3, contour.constData(), contour.size()))
			return true;
	}
	return false;
}

SphericalPolygon SphericalConvexPolygon::toSphericalPolygon() const
{
	return SphericalPolygon(getConvexContour());
}


//! Compute the intersection of the planes defined by the 2 halfspaces on the sphere (usually on 2 points) and return it in p1 and p2.
//! If the 2 SphericalCaps don't interesect or intersect only at 1 point, false is returned and p1 and p2 are undefined
bool planeIntersect2(const SphericalCap& h1, const SphericalCap& h2, Vec3d& p1, Vec3d& p2)
{
	if (!h1.intersects(h2))
		return false;
	const Vec3d& n1 = h1.n;
	const Vec3d& n2 = h2.n;
	const double& d1 = -h1.d;
	const double& d2 = -h2.d;
	const double& a1 = n1[0];
	const double& b1 = n1[1];
	const double& c1 = n1[2];
	const double& a2 = n2[0];
	const double& b2 = n2[1];
	const double& c2 = n2[2];

	Q_ASSERT(fabs(n1.lengthSquared()-1.)<0.000001);
	Q_ASSERT(fabs(n2.lengthSquared()-1.)<0.000001);

	// Compute the parametric equation of the line at the intersection of the 2 planes
	Vec3d u = n1^n2;
	if (u[0]==0. && u[1]==0. && u[2]==0.)
	{
		// The planes are parallel
		return false;
	}
	u.normalize();

	// u gives the direction of the line, still need to find a suitable start point p0
	// Find the axis on which the line varies the fastest, and solve the system for value == 0 on this axis
	int maxI = (fabs(u[0])>=fabs(u[1])) ? (fabs(u[0])>=fabs(u[2]) ? 0 : 2) : (fabs(u[2])>fabs(u[1]) ? 2 : 1);
	Vec3d p0(0);
	switch (maxI)
	{
		case 0:
		{
			// Intersection of the line with the plane x=0
			const double denom = b1*c2-b2*c1;
			p0[1] = (d2*c1-d1*c2)/denom;
			p0[2] = (d1*b2-d2*b1)/denom;
			break;
		}
		case 1:
		{
			// Intersection of the line with the plane y=0
			const double denom = a1*c2-a2*c1;
			p0[0]=(c1*d2-c2*d1)/denom;
			p0[2]=(a2*d1-d2*a1)/denom;
			break;
		}
		case 2:
		{
			// Intersection of the line with the plane z=0
			const double denom = a1*b2-a2*b1;
			p0[0]=(b1*d2-b2*d1)/denom;
			p0[1]=(a2*d1-a1*d2)/denom;
			break;
		}
	}

	// The intersection line is now fully defined by the parametric equation p = p0 + u*t

	// The points are on the unit sphere x^2+y^2+z^2=1, replace x, y and z by the parametric equation to get something of the form at^2+b*t+c=0
	// const double a = 1.;
	const double b = p0*u*2.;
	const double c = p0.lengthSquared()-1.;

	// If discriminant <=0, zero or 1 real solution
	const double D = b*b-4.*c;
	if (D<=0.)
		return false;

	const double sqrtD = std::sqrt(D);
	const double t1 = (-b+sqrtD)/2.;
	const double t2 = (-b-sqrtD)/2.;
	p1 = p0+u*t1;
	p2 = p0+u*t2;

	Q_ASSERT(fabs(p1.lengthSquared()-1.)<0.000001);
	Q_ASSERT(fabs(p2.lengthSquared()-1.)<0.000001);

	return true;
}

Vec3d greatCircleIntersection(const Vec3d& p1, const Vec3d& p2, const Vec3d& p3, const Vec3d& p4, bool& ok)
{
	Vec3d n2 = p3^p4;
	n2.normalize();
	return greatCircleIntersection(p1, p2, n2, ok);
}

Vec3d greatCircleIntersection(const Vec3d& p1, const Vec3d& p2, const Vec3d& n2, bool& ok)
{
	Vec3d n1 = p1^p2;
	Q_ASSERT(std::fabs(n2.lengthSquared()-1.)<0.00000001);
	n1.normalize();
	// Compute the parametric equation of the line at the intersection of the 2 planes
	Vec3d u = n1^n2;
	if (u.length()<1e-7)
	{
		// The planes are parallel
		ok = false;
		return u;
	}
	u.normalize();

	// The 2 candidates are u and -u. Now need to find which point is the correct one.
	ok = true;

	n1 = p1; n1+=p2;
	n1.normalize();
	if (n1*u>0.)
		return u;
	else
		return -u;
}

SphericalRegionP SphericalRegion::loadFromJson(QIODevice* in)
{
	StelJsonParser parser;
	return loadFromQVariant(parser.parse(*in).toMap());
}

SphericalRegionP SphericalRegion::loadFromJson(const QByteArray& a)
{
	QBuffer buf;
	buf.setData(a);
	buf.open(QIODevice::ReadOnly);
	return loadFromJson(&buf);
}

inline void parseRaDec(const QVariant& vRaDec, Vec3d& v)
{
	const QVariantList& vl = vRaDec.toList();
	bool ok;
	if (vl.size()!=2)
		throw std::runtime_error(qPrintable(QString("invalid Ra,Dec pair: \"%1\" (expect 2 double values in degree, got %2)").arg(vRaDec.toString()).arg(vl.size())));
	StelUtils::spheToRect(vl.at(0).toDouble(&ok)*M_PI/180., vl.at(1).toDouble(&ok)*M_PI/180., v);
	if (!ok)
		throw std::runtime_error(qPrintable(QString("invalid Ra,Dec pair: \"%1\" (expect 2 double values in degree)").arg(vRaDec.toString())));
}

SphericalRegionP SphericalRegion::loadFromQVariant(const QVariantMap& map)
{
	QVariantList contoursList = map.value("skyConvexPolygons").toList();
	if (contoursList.empty())
		contoursList = map.value("worldCoords").toList();
	else
		qWarning() << "skyConvexPolygons in preview JSON files is deprecated. Replace with worldCoords.";

	if (contoursList.empty())
		throw std::runtime_error("missing sky contours description required for Spherical Geometry elements.");

	// Load the matching textures positions (if any)
	QVariantList texCoordList = map.value("textureCoords").toList();
	if (!texCoordList.isEmpty() && contoursList.size()!=texCoordList.size())
		throw std::runtime_error(qPrintable(QString("the number of sky contours (%1) does not match the number of texture space contours (%2)").arg( contoursList.size()).arg(texCoordList.size())));

	bool ok;
	if (texCoordList.isEmpty())
	{
		// No texture coordinates
		QVector<QVector<Vec3d> > contours;
		QVector<Vec3d> vertices;
		for (int i=0;i<contoursList.size();++i)
		{
			const QVariantList& contourToList = contoursList.at(i).toList();
			if (contourToList.size()<1)
				throw std::runtime_error(qPrintable(QString("invalid contour definition: %1").arg(contoursList.at(i).toString())));
			if (contourToList.at(0).toString()=="CAP")
			{
				// We now parse a cap, the format is "CAP",[ra, dec],aperture
				if (contourToList.size()!=3)
					throw std::runtime_error(qPrintable(QString("invalid CAP description: %1 (expect \"CAP\",[ra, dec],aperture)").arg(contoursList.at(i).toString())));
				Vec3d v;
				parseRaDec(contourToList.at(1), v);
				double d = contourToList.at(2).toDouble(&ok)*M_PI/180.;
				if (!ok)
					throw std::runtime_error(qPrintable(QString("invalid aperture angle: \"%1\" (expect a double value in degree)").arg(contourToList.at(2).toString())));
				SphericalCap cap(v,std::cos(d));
				contours.append(cap.toSphericalConvexPolygon().getConvexContour());
				vertices.clear();
				continue;
			}
			// If no type is provided, assume a polygon
			if (contourToList.size()<3)
				throw std::runtime_error("a polygon contour must have at least 3 vertices");
			Vec3d v;
			foreach (const QVariant& vRaDec, contourToList)
			{
				parseRaDec(vRaDec, v);
				vertices.append(v);
			}
			Q_ASSERT(vertices.size()>2);
			contours.append(vertices);
			vertices.clear();
		}
		return SphericalRegionP(new SphericalPolygon(contours));
	}
	else
	{
		// With texture coordinates
		QVector<QVector<SphericalTexturedPolygon::TextureVertex> > contours;
		QVector<SphericalTexturedPolygon::TextureVertex> vertices;
		for (int i=0;i<contoursList.size();++i)
		{
			// Load vertices
			const QVariantList& polyRaDecToList = contoursList.at(i).toList();
			if (polyRaDecToList.size()<3)
				throw std::runtime_error("a polygon contour must have at least 3 vertices");
			SphericalTexturedPolygon::TextureVertex v;
			foreach (const QVariant& vRaDec, polyRaDecToList)
			{
				parseRaDec(vRaDec, v.vertex);
				vertices.append(v);
			}
			Q_ASSERT(vertices.size()>2);

			// Add the texture coordinates
			const QVariantList& polyXYToList = texCoordList.at(i).toList();
			if (polyXYToList.size()!=vertices.size())
				throw std::runtime_error("texture coordinate and vertices number mismatch for contour");
			for (int n=0;n<polyXYToList.size();++n)
			{
				const QVariantList& vl = polyXYToList.at(n).toList();
				if (vl.size()!=2)
					throw std::runtime_error("invalid texture coordinate pair (expect 2 double values in degree)");
				vertices[n].texCoord.set(vl.at(0).toDouble(&ok), vl.at(1).toDouble(&ok));
				if (!ok)
					throw std::runtime_error("invalid texture coordinate pair (expect 2 double values in degree)");
			}
			contours.append(vertices);
			vertices.clear();
		}
		return SphericalRegionP(new SphericalTexturedPolygon(contours));
	}
	Q_ASSERT(0);
	return SphericalRegionP(new SphericalCap());
}

///////////////////////////////////////////////////////////////////////////////
// Serialization into QVariant

QVariantMap SphericalPoint::toQVariant() const
{
	QVariantMap res;
	res.insert("type", "POINT");
	double ra, dec;
	StelUtils::rectToSphe(&ra, &dec, n);
	QVariantList l;
	l << ra*180./M_PI << dec*180./M_PI;
	res.insert("pos", l);
	return res;
}

QVariantMap SphericalCap::toQVariant() const
{
	QVariantMap res;
	res.insert("type", "CAP");
	double ra, dec;
	StelUtils::rectToSphe(&ra, &dec, n);
	QVariantList l;
	l << ra*180./M_PI << dec*180./M_PI;
	res.insert("center", l);
	res.insert("radius", std::acos(d)*180./M_PI);
	return res;
}

QVariantMap AllSkySphericalRegion::toQVariant() const
{
	QVariantMap res;
	res.insert("type", "ALLSKY");
	return res;
}

QVariantMap SphericalPolygon::toQVariant() const
{
	QVariantMap res;
	QVariantList worldCoordinates;
	double ra, dec;
	foreach (const QVector<Vec3d>& contour, getSimplifiedContours())
	{
		QVariantList cv;
		foreach (const Vec3d& v, contour)
		{
			StelUtils::rectToSphe(&ra, &dec, v);
			QVariantList vv;
			vv << ra*180./M_PI << dec*180./M_PI;
			cv.append((QVariant)vv);
		}
		worldCoordinates.append((QVariant)cv);
	}
	res.insert("worldCoords", worldCoordinates);
	return res;
}

QVariantMap SphericalTexturedPolygon::toQVariant() const
{
	Q_ASSERT(0);
	// TODO store a tesselated polygon?, including edge flags?
	return QVariantMap();
}

QVariantMap SphericalConvexPolygon::toQVariant() const
{
	QVariantMap res;
	res.insert("type", "CVXPOLYGON");
	QVariantList cv;
	double ra, dec;
	foreach (const Vec3d& v, contour)
	{
		StelUtils::rectToSphe(&ra, &dec, v);
		QVariantList vv;
		vv << ra*180./M_PI << dec*180./M_PI;
		cv.append((QVariant)vv);
	}
	res.insert("worldCoords", cv);
	return res;
}

QVariantMap SphericalTexturedConvexPolygon::toQVariant() const
{
	QVariantMap res = SphericalConvexPolygon::toQVariant();
	QVariantList cv;
	foreach (const Vec2f& v, textureCoords)
	{
		QVariantList vv;
		vv << v[0] << v[1];
		cv.append((QVariant)vv);
	}
	res.insert("textureCoords", cv);
	return res;
}

static int getSide(const Vec3d& v, int onLine)
{
	Q_ASSERT(onLine>=0 && onLine<=3);
	return v[onLine]>0 ? 0 : 1;
}

static void interpolateAtCrossingPoint(const Vec3d& v1, const Vec3d& v2, const Vec3d& planDirection, Vec3d& result)
{
	bool ok;
	result=greatCircleIntersection(v1, v2, planDirection, ok);
	Q_ASSERT(ok);
}

QString SubContour::toJSON() const
{
	QString res("[");
	double ra, dec;
	foreach (const EdgeVertex& v, *this)
	{
		StelUtils::rectToSphe(&ra, &dec, v.vertex);
		res += QString(" [") + QString::number(ra*180./M_PI) + "," + QString::number(dec*180./M_PI) + ", " + (v.edgeFlag ? QString("true"): QString("false")) + "],";
	}
	res[res.size()-1]=' ';
	res.append(']');
	return res;
};

void OctahedronContour::splitContourByPlan(int onLine, const SubContour& inputContour, QVector<SubContour> result[2])
{
	SubContour currentSubContour;
	SubContour unfinishedSubContour;
	int previousQuadrant=getSide(inputContour.first().vertex, onLine);
	int currentQuadrant=0;
	Vec3d tmpVertex;
	EdgeVertex previousVertex=inputContour.first();
	EdgeVertex currentVertex;
	int i;
	const Vec3d plan(onLine==0?1:0, onLine==1?1:0, onLine==2?1:0);
	// Take care first of the unfinished contour
	for (i=0;i<inputContour.size();++i)
	{
		currentVertex = inputContour.at(i);
		currentQuadrant = getSide(currentVertex.vertex, onLine);
		if (currentQuadrant==previousQuadrant)
		{
			unfinishedSubContour << currentVertex;
		}
		else
		{
			// We crossed the line
			interpolateAtCrossingPoint(previousVertex.vertex, currentVertex.vertex, plan, tmpVertex);
			unfinishedSubContour << EdgeVertex(tmpVertex, false); // Last point of the contour, it's not an edge
			Q_ASSERT(currentSubContour.isEmpty());
			currentSubContour << EdgeVertex(tmpVertex, previousVertex.edgeFlag);
			previousQuadrant = currentQuadrant;
			break;
		}
		previousVertex=currentVertex;
	}
	// Now handle the other ones
	for (;i<inputContour.size();++i)
	{
		currentVertex = inputContour.at(i);
		currentQuadrant = getSide(currentVertex.vertex, onLine);
		if (currentQuadrant==previousQuadrant)
		{
			currentSubContour << currentVertex;
		}
		else
		{
			// We crossed the line
			interpolateAtCrossingPoint(previousVertex.vertex, currentVertex.vertex, plan, tmpVertex);
			currentSubContour << EdgeVertex(tmpVertex, false); // Last point of the contour, it's not an edge
			result[previousQuadrant] << currentSubContour;
			currentSubContour.clear();
			currentSubContour << EdgeVertex(tmpVertex, previousVertex.edgeFlag);
			currentSubContour << currentVertex;
		}
		previousVertex=currentVertex;
	}

	// Handle the last line between the last and first point
	previousQuadrant = currentQuadrant;
	currentQuadrant = getSide(inputContour.first().vertex, onLine);
	if (currentQuadrant==previousQuadrant)
	{
	}
	else
	{
		// We crossed the line
		interpolateAtCrossingPoint(previousVertex.vertex, inputContour.first().vertex, plan, tmpVertex);
		currentSubContour << EdgeVertex(tmpVertex, false);	// Last point of the contour, it's not an edge
		result[previousQuadrant] << currentSubContour;
		currentSubContour.clear();
		currentSubContour << EdgeVertex(tmpVertex, previousVertex.edgeFlag);
	}

	// Append the last contour made from the last vertices + the previous unfinished ones
	currentSubContour << unfinishedSubContour;
	result[currentQuadrant] << currentSubContour;
}

SubContour::SubContour(const QVector<Vec3d>& vertices, bool closed) : QVector<EdgeVertex>(vertices.size(), EdgeVertex(true))
{
	// Create the contour list by adding the matching edge flags
	for (int i=0;i<vertices.size();++i)
		(*this)[i].vertex = vertices.at(i);
	if (closed=false)
	{
		this->last().edgeFlag=false;
	}
}

SubContour SubContour::reversed() const
{
	SubContour res;
	QVectorIterator<EdgeVertex> iter(*this);
	iter.toBack();
	while (iter.hasPrevious())
		res.append(iter.previous());
	return res;
}

OctahedronContour::OctahedronContour(const SubContour& initContour) : tesselated(false)
{
	sides.resize(8);

	QVector<SubContour> splittedContour1[2];
	// Split the contour on the plan Y=0
	splitContourByPlan(1, initContour, splittedContour1);
	// Re-split the contours on the plan X=0
	QVector<SubContour> splittedVertices2[4];
	foreach (const SubContour& subContour, splittedContour1[0])
		splitContourByPlan(0, subContour, splittedVertices2);
	foreach (const SubContour& subContour, splittedContour1[1])
		splitContourByPlan(0, subContour, splittedVertices2+2);

	// Now complete the contours which cross the areas from one side to another by adding poles
	for (int c=0;c<4;++c)
	{
		for (int i=0;i<splittedVertices2[c].size();++i)
		{
			SubContour& tmpSubContour = splittedVertices2[c][i];
			Vec3d v = tmpSubContour.first().vertex^tmpSubContour.last().vertex;
			if (v[2]>0.0000001)
			{
				// A south pole has to be added
				tmpSubContour << EdgeVertex(Vec3d(0,0,-1), false);
			}
			else if (v[2]<-0.0000001)
			{
				// A north pole has to be added
				tmpSubContour << EdgeVertex(Vec3d(0,0,1), false);
			}
			else
			{
				// else the contour ends on the same longitude line as it starts
				Q_ASSERT(std::fabs(tmpSubContour.first().vertex.longitude()-tmpSubContour.last().vertex.longitude()<0.00000001));
			}
		}
		foreach (const SubContour& subContour, splittedVertices2[c])
		{
			splitContourByPlan(2, subContour, sides.data()+c*2);
		}
	}

	projectOnOctahedron();
}

void OctahedronContour::append(const OctahedronContour& other)
{
	Q_ASSERT(sides.size()==8 && other.sides.size()==8);
	for (int i=0;i<8;++i)
	{
		sides[i] += other.sides[i];
	}
	tesselated = false;
}

void OctahedronContour::appendReversed(const OctahedronContour& other)
{
	Q_ASSERT(sides.size()==8 && other.sides.size()==8);
	for (int i=0;i<8;++i)
	{
		foreach (const SubContour& sub, other.sides[i])
		{
			sides[i] += sub.reversed();
		}
	}
	tesselated = false;
}

void OctahedronContour::projectOnOctahedron()
{
	static const Vec3d faceCenter[] = {	Vec3d(1,1,1), Vec3d(1,1,-1),Vec3d(-1,1,1),Vec3d(-1,1,-1),
		Vec3d(1,-1,1),Vec3d(1,-1,-1),Vec3d(-1,-1,1),Vec3d(-1,-1,-1)};

	Q_ASSERT(sides.size()==8);
	QVector<SubContour>* subs = sides.data();

	for (int i=0;i<8;++i)
	{
		for (QVector<SubContour>::Iterator iter=subs[i].begin();iter!=subs[i].end();++iter)
		{
			for (SubContour::Iterator v=iter->begin();v!=iter->end();++v)
			{
				// Project on the face with aperture = 90 deg
				v->vertex *= 1./(faceCenter[i]*v->vertex);
				// May want to add offsets after that to map TOAST projection
			}
		}
	}
}

StelVertexArray OctahedronContour::getTesselatedTriangles()
{
	if (!tesselated)
		tesselate(WindingPositive);
	StelVertexArray result;
	Q_ASSERT(sides.size()==8);
	QVector<SubContour>* subs = sides.data();
	for (int i=0;i<8;++i)
	{
		for (QVector<SubContour>::Iterator iter=subs[i].begin();iter!=subs[i].end();++iter)
		{
			for (SubContour::Iterator v=iter->begin();v!=iter->end();++v)
			{
				result.vertex.append(v->vertex);
				result.vertex.last().normalize();
				result.edgeFlags.append(v->edgeFlag);
			}
		}
	}
	return result;
}

// Store data for the GLUES tesselation callbacks
struct OctTessCallbackData
{
	SubContour result;		//! Reference to the instance of OctahedronContour being tesselated.
	bool edgeFlag;					//! Used to store temporary edgeFlag found by the tesselator.
	QList<Vec3d> tempVertices;	//! Used to store the temporary combined vertices
};

void octVertexCallback(void* vertexData, void* userData)
{
	QVector<EdgeVertex>& res = ((OctTessCallbackData*)userData)->result;
	const double* v = (double*)vertexData;
	Vec3d vv(v[0], v[1], v[2]);
	res.append(EdgeVertex(vv, ((OctTessCallbackData*)userData)->edgeFlag));
}

void octEdgeFlagCallback(GLboolean flag, void* userData)
{
	((OctTessCallbackData*)userData)->edgeFlag=flag;
}

void octCombineCallback(double coords[3], void* vertex_data[4], GLfloat weight[4], void** outData, void* userData)
{
	QList<Vec3d>& tempVertices = ((OctTessCallbackData*)userData)->tempVertices;
	Vec3d newVertex(0.);
	for (int i=0;i<4;++i)
	{
		if (vertex_data[i]==NULL)
			break;
		double* dd = (double*)vertex_data[i];
		newVertex+=Vec3d(dd[i]*weight[i],dd[i]*weight[i],dd[i]*weight[i]);
	}
	tempVertices.append(newVertex);
	*outData = tempVertices.last();
}

void octCheckBeginCallback(GLenum type)
{
	Q_ASSERT(type==GL_TRIANGLES);
}

SubContour tesselateOneSide(GLUEStesselator* tess, const QVector<SubContour>& contours)
{
	OctTessCallbackData data;
	gluesTessBeginPolygon(tess, &data);
	for (int c=0;c<contours.size();++c)
	{
		gluesTessBeginContour(tess);
		foreach (const EdgeVertex& v, contours.at(c))
		{
                        gluesTessVertex(tess, const_cast<double*>((const double*)v.vertex), const_cast<void*>((const void*)v.vertex));
		}
		gluesTessEndContour(tess);
	}
	gluesTessEndPolygon(tess);
	return data.result;
}

void OctahedronContour::tesselate(TessWindingRule windingRule) const
{
	// Use GLUES tesselation functions to transform the polygon into a list of triangles
	GLUEStesselator* tess = gluesNewTess();
#ifndef NDEBUG
	gluesTessCallback(tess, GLUES_TESS_BEGIN, (GLvoid(*)()) &octCheckBeginCallback);
#endif
	gluesTessCallback(tess, GLUES_TESS_VERTEX_DATA, (GLvoid(*)()) &octVertexCallback);
	gluesTessCallback(tess, GLUES_TESS_EDGE_FLAG_DATA, (GLvoid(*)()) &octEdgeFlagCallback);
	gluesTessCallback(tess, GLUES_TESS_ERROR, (GLvoid(*)()) &errorCallback);
	gluesTessCallback(tess, GLUES_TESS_COMBINE_DATA, (GLvoid(*)()) &octCombineCallback);
        const double windRule = (windingRule==OctahedronContour::WindingPositive) ? GLUES_TESS_WINDING_POSITIVE : GLUES_TESS_WINDING_ABS_GEQ_TWO;
	gluesTessProperty(tess, GLUES_TESS_WINDING_RULE, windRule);
	gluesTessNormal(tess, 0,0,1);

	// Call the tesselator on each side
	for (int i=0;i<8;++i)
	{
		QVector<SubContour> res;
		res.append(tesselateOneSide(tess, sides[i]));
		sides[i]=res;
	}

	gluesDeleteTess(tess);
	tesselated = true;
}


void OctahedronContour::inPlaceIntersection(const OctahedronContour& mpoly)
{
	if (!tesselated)
		tesselate(WindingPositive);
	if (!mpoly.tesselated)
		mpoly.tesselate(WindingPositive);
	append(mpoly);
	tesselate(WindingAbsGeqTwo);
}

void OctahedronContour::inPlaceUnion(const OctahedronContour& mpoly)
{
	append(mpoly);
}

void OctahedronContour::inPlaceSubtraction(const OctahedronContour& mpoly)
{
	if (!tesselated)
		tesselate(WindingPositive);
	if (!mpoly.tesselated)
		mpoly.tesselate(WindingPositive);
	appendReversed(mpoly);
	tesselate(WindingPositive);
}
